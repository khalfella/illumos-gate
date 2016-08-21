#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <stddef.h>
#include <limits.h>

#include <rctl.h>


#include <regex.h>


#include <ctype.h>

#include "projent.h"

#define BOSTR_REG_EXP	"^"
#define EOSTR_REG_EXP	"$"
#define EQUAL_REG_EXP	"="
#define STRNG_REG_EXP	".*"
#define STRN0_REG_EXP	"(.*)"

#define IDENT_REG_EXP	"[[:alpha:]][[:alnum:]_.-]*"
#define STOCK_REG_EXP	"([[:upper:]]{1,5}(.[[:upper:]]{1,5})?,)?"

#define FLTNM_REG_EXP	"([[:digit:]]+(\\.[[:digit:]]+)?)"
#define	MODIF_REG_EXP	"([kmgtpe])?"
#define UNIT__REG_EXP	"([bs])?"

#define TOKEN_REG_EXP	"[[:alnum:]_./=+-]*"

#define ATTRB_REG_EXP	"(" STOCK_REG_EXP IDENT_REG_EXP ")"
#define ATVAL_REG_EXP	ATTRB_REG_EXP EQUAL_REG_EXP STRN0_REG_EXP
#define VALUE_REG_EXP	FLTNM_REG_EXP MODIF_REG_EXP UNIT__REG_EXP

#define TO_EXP(X)	BOSTR_REG_EXP X EOSTR_REG_EXP

#define ATTRB_EXP	TO_EXP(ATTRB_REG_EXP)
#define ATVAL_EXP	TO_EXP(ATVAL_REG_EXP)
#define VALUE_EXP	TO_EXP(VALUE_REG_EXP)
#define TOKEN_EXP	TO_EXP(TOKEN_REG_EXP)


#define MAX_OF(X,Y)	(((X) > (Y)) ? (X) : (Y))


#define SEQUAL(X,Y)	(strcmp((X), (Y)) == 0)


#define BYTES_SCALE	1
#define SCNDS_SCALE	2

#define	RCTL_TYPE_UNKWN	0x00
#define	RCTL_TYPE_BYTES	0x01
#define	RCTL_TYPE_SCNDS	0x02
#define	RCTL_TYPE_COUNT	0x03

#define	RCTL_PRIV_PRIV	0x01
#define	RCTL_PRIV_PRIVD	0x02
#define	RCTL_PRIV_BASIC	0x04
#define	RCTL_PRIV_ALL	0x07

#define RCTL_ACTN_NONE	0x01
#define RCTL_ACTN_DENY	0x02
#define RCTL_ACTN_SIG	0x04
#define RCTL_ACTN_ALL	0x07

#define	RCTL_SIG_ABRT	0x01
#define	RCTL_SIG_XRES	0x02
#define	RCTL_SIG_HUP	0x04
#define	RCTL_SIG_STOP	0x08
#define	RCTL_SIG_TERM	0x10
#define	RCTL_SIG_KILL	0x20
#define	RCTL_SIG_XFSZ	0x40
#define	RCTL_SIG_XCPU	0x80

#define	RCTL_SIG_CMN	0x3f
#define	RCTL_SIG_ALL	0xff

typedef struct sig_s {
	char	sig[6];
	int	mask;
} sig_t;

typedef struct rctlrule_s {
	uint64_t rtcl_max;
	uint8_t  rtcl_type;
	uint8_t  rctl_privs;
	uint8_t  rctl_action;
	uint8_t  rctl_sigs;
} rctlrule_t;


#define SIGS_CNT	16
sig_t sigs[SIGS_CNT] = {
	/* Signal names */
	{"ABRT", RCTL_SIG_ABRT},
	{"XRES", RCTL_SIG_XRES},
	{"HUP",  RCTL_SIG_HUP},
	{"STOP", RCTL_SIG_STOP},
	{"TERM", RCTL_SIG_TERM},
	{"KILL", RCTL_SIG_KILL},
	{"XFSZ", RCTL_SIG_XFSZ},
	{"XCPU", RCTL_SIG_XCPU},

	/* Singnal numbers */
	{"6",  RCTL_SIG_ABRT},
	{"38", RCTL_SIG_XRES},
	{"1",  RCTL_SIG_HUP},
	{"23", RCTL_SIG_STOP},
	{"15", RCTL_SIG_TERM},
	{"9",  RCTL_SIG_KILL},
	{"31", RCTL_SIG_XFSZ},
	{"30", RCTL_SIG_XCPU},
};


rctlrule_t allrules =  {
	UINT64_MAX,
	RCTL_TYPE_UNKWN,
	RCTL_PRIV_ALL,
	RCTL_ACTN_ALL,
	RCTL_SIG_ALL,
};


int
rctl_get_info(char *name, rctl_info_t *pinfo)
{
	rctlblk_t *blk1, *blk2, *tmp;
	rctl_priv_t priv;

	blk1 = blk2 = tmp = NULL;
	blk1 = util_safe_malloc(rctlblk_size());
	blk2 = util_safe_malloc(rctlblk_size());

	if (getrctl(name, NULL, blk1, RCTL_FIRST) == 0) {
		priv = rctlblk_get_privilege(blk1);
		while (priv != RCPRIV_SYSTEM) {
			tmp = blk2;
			blk2 = blk1;
			blk1 = tmp;
			if (getrctl(name, blk2, blk1, RCTL_NEXT) != 0) {
				goto out;
			}
			priv = rctlblk_get_value(blk1);
		}


		pinfo->value = rctlblk_get_value(blk1);
		pinfo->flags = rctlblk_get_global_flags(blk1);
		return (0);
	}

out:
	free(blk1);
	free(blk2);
	return (1);
}

rctlrule_t
*rctl_get_rule(rctl_info_t *pinfo)
{
	rctlrule_t *ret;

	ret = util_safe_malloc(sizeof(rctlrule_t));

	ret->rtcl_max = pinfo->value;
	if (pinfo->flags & RCTL_GLOBAL_BYTES) {
		ret->rtcl_type = RCTL_TYPE_BYTES;
	} else if (pinfo->flags & RCTL_GLOBAL_SECONDS) {
		ret->rtcl_type = RCTL_TYPE_SCNDS;
	} else if (pinfo->flags & RCTL_GLOBAL_COUNT) {
		ret->rtcl_type = RCTL_TYPE_COUNT;
	} else {
		ret->rtcl_type = RCTL_TYPE_UNKWN;
	}


	if (pinfo->flags & RCTL_GLOBAL_NOBASIC) {
		ret->rctl_privs = RCTL_PRIV_PRIV | RCTL_PRIV_PRIVD;
	} else {
		ret->rctl_privs = RCTL_PRIV_ALL;
	}

	if (pinfo->flags & RCTL_GLOBAL_DENY_ALWAYS) {
		ret->rctl_action = RCTL_ACTN_DENY;
	} else if (pinfo->flags & RCTL_GLOBAL_DENY_NEVER) {
		ret->rctl_action = RCTL_ACTN_NONE;
	} else {
		ret->rctl_action = RCTL_ACTN_NONE | RCTL_ACTN_DENY;
	}

	if (pinfo->flags & RCTL_GLOBAL_SIGNAL_NEVER) {
		ret->rctl_sigs = 0;
	} else {
		ret->rctl_action |= RCTL_ACTN_SIG;
		ret->rctl_sigs = RCTL_SIG_CMN;
		if (pinfo->flags & RCTL_GLOBAL_CPU_TIME) {
			ret->rctl_sigs |= RCTL_SIG_XCPU;
		}
		if (pinfo->flags & RCTL_GLOBAL_FILE_SIZE) {
			ret->rctl_sigs |= RCTL_SIG_XFSZ;
		}
	}

	return (ret);
}
