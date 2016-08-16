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
