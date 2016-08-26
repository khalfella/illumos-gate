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

/*
#include "projent.h"
*/

#include "util.h"
#include "rctl.h"



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

void
rctl_get_rule(rctl_info_t *pinfo, rctlrule_t* prule)
{

	prule->rtcl_max = pinfo->value;
	if (pinfo->flags & RCTL_GLOBAL_BYTES) {
		prule->rtcl_type = RCTL_TYPE_BYTES;
	} else if (pinfo->flags & RCTL_GLOBAL_SECONDS) {
		prule->rtcl_type = RCTL_TYPE_SCNDS;
	} else if (pinfo->flags & RCTL_GLOBAL_COUNT) {
		prule->rtcl_type = RCTL_TYPE_COUNT;
	} else {
		prule->rtcl_type = RCTL_TYPE_UNKWN;
	}


	if (pinfo->flags & RCTL_GLOBAL_NOBASIC) {
		prule->rctl_privs = RCTL_PRIV_PRIV | RCTL_PRIV_PRIVD;
	} else {
		prule->rctl_privs = RCTL_PRIV_ALL;
	}

	if (pinfo->flags & RCTL_GLOBAL_DENY_ALWAYS) {
		prule->rctl_action = RCTL_ACTN_DENY;
	} else if (pinfo->flags & RCTL_GLOBAL_DENY_NEVER) {
		prule->rctl_action = RCTL_ACTN_NONE;
	} else {
		prule->rctl_action = RCTL_ACTN_NONE | RCTL_ACTN_DENY;
	}

	if (pinfo->flags & RCTL_GLOBAL_SIGNAL_NEVER) {
		prule->rctl_sigs = 0;
	} else {
		prule->rctl_action |= RCTL_ACTN_SIG;
		prule->rctl_sigs = RCTL_SIG_CMN;
		if (pinfo->flags & RCTL_GLOBAL_CPU_TIME) {
			prule->rctl_sigs |= RCTL_SIG_XCPU;
		}
		if (pinfo->flags & RCTL_GLOBAL_FILE_SIZE) {
			prule->rctl_sigs |= RCTL_SIG_XFSZ;
		}
	}

}
