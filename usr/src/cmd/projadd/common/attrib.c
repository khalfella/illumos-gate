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

#include <sys/debug.h>

#include "attrib.h"
#include "resctl.h"
#include "util.h"

#define	BOSTR_REG_EXP	"^"
#define	EOSTR_REG_EXP	"$"
#define	EQUAL_REG_EXP	"="
#define STRN0_REG_EXP	"(.*)"
#define	IDENT_REG_EXP	"[[:alpha:]][[:alnum:]_.-]*"
#define INTNM_REG_EXP	"[[:digit:]]+"
#define SIGAC_REG_EXP	"sig(nal)?(=.*)?"
#define SIGHD_REG_EXP	"(signal|sig)"
#define SIGVL_REG_EXP	"(([[:digit:]]+)|((SIG)?([[:upper:]]+)([+-][123])?))"
#define SIGNL_REG_EXP	SIGHD_REG_EXP EQUAL_REG_EXP SIGVL_REG_EXP
#define STOCK_REG_EXP	"([[:upper:]]{1,5}(.[[:upper:]]{1,5})?,)?"
#define ATTRB_REG_EXP	"(" STOCK_REG_EXP IDENT_REG_EXP ")"
#define ATVAL_REG_EXP	ATTRB_REG_EXP EQUAL_REG_EXP STRN0_REG_EXP

#define TO_EXP(X)	BOSTR_REG_EXP X EOSTR_REG_EXP

#define POOLN_EXP	TO_EXP(IDENT_REG_EXP)
#define INTNM_EXP	TO_EXP(INTNM_REG_EXP)
#define SIGAC_EXP	TO_EXP(SIGAC_REG_EXP)
#define SIGNL_EXP	TO_EXP(SIGNL_REG_EXP)
#define ATTRB_EXP	TO_EXP(ATTRB_REG_EXP)
#define ATVAL_EXP	TO_EXP(ATVAL_REG_EXP)

#define MAX_OF(X,Y)	(((X) > (Y)) ? (X) : (Y))


#define SEQU(X,Y)		(strcmp((X), (Y)) == 0)
#define	SIN1(X, S1)		((SEQU((X), (S1))))
#define	SIN2(X, S1, S2)		((SEQU((X), (S1))) || (SIN1((X), (S2))))
#define	SIN3(X, S1, S2, S3)	((SEQU((X), (S1))) || (SIN2((X), (S2), (S3))))

#define	ATT_VAL_TYPE_NULL	0
#define ATT_VAL_TYPE_VALUE	1
#define ATT_VAL_TYPE_LIST	2

#define ATT_ALLOC()		attrib_alloc()
#define ATT_VAL_ALLOC(T, V)	attrib_val_alloc((T), (V))
#define ATT_VAL_ALLOC_NULL()	ATT_VAL_ALLOC(ATT_VAL_TYPE_NULL, NULL)
#define ATT_VAL_ALLOC_VALUE(V)	ATT_VAL_ALLOC(ATT_VAL_TYPE_VALUE, (V))
#define ATT_VAL_ALLOC_LIST(L)	ATT_VAL_ALLOC(ATT_VAL_TYPE_LIST, (L))

typedef struct attrib_val_s {
	int att_val_type;
	/*LINTED*/
	union {
		char *att_val_value;
		lst_t *att_val_values;
        };
} attrib_val_t;

typedef struct attrib_s {
	char *att_name;
	attrib_val_t *att_value;
} attrib_t;


attrib_t *attrib_alloc();
attrib_val_t *attrib_val_alloc(int, void *);
char *attrib_val_tostring(attrib_val_t *, boolean_t);

int
attrib_validate_rctl(attrib_t *att, resctlrule_t *rule, lst_t *errlst)
{
	int ret = 0;
	char *atname = att->att_name;
	attrib_val_t *atv, *atval = att->att_value;
	attrib_val_t *priv, *val, *action;
	char *vpriv, *vval, *vaction, *sigstr;
	int atv_type = atval->att_val_type;
	char *str;
	int i, j, k;

	int nmatch;
	uint8_t rpriv, raction, sigval;
	uint64_t rmax;
	regex_t pintexp, sigacexp, signlexp;
	regmatch_t *mat;

	int nonecount, denycount, sigcount;

	if (regcomp(&pintexp, INTNM_EXP, REG_EXTENDED) != 0) {
			util_add_errmsg(errlst, gettext(
			    "Failed to compile regex for pos. integer"));
			return (1);
	}

	if (regcomp(&sigacexp, SIGAC_EXP, REG_EXTENDED) != 0) {
			util_add_errmsg(errlst, gettext(
			    "Failed to compile regex for sigaction"));
			regfree(&pintexp);
			return (1);
	}

	if (regcomp(&signlexp, SIGNL_EXP, REG_EXTENDED) != 0) {
			util_add_errmsg(errlst, gettext(
			    "Failed to compile regex for signal"));
			regfree(&pintexp);
			regfree(&sigacexp);
			return (1);
	}

	nmatch = signlexp.re_nsub + 1;
	mat = util_safe_zmalloc(nmatch * sizeof(regmatch_t));

	if (atv_type == ATT_VAL_TYPE_NULL) {
		goto out;
	} else if (atv_type == ATT_VAL_TYPE_VALUE) {
		util_add_errmsg(errlst, gettext(
		    "rctl \"%s\" missing value"), atname);
		ret = 1;
		goto out;
	}

	for (i = 0; i < lst_size(atval->att_val_values); i++) {
		atv = lst_at(atval->att_val_values, i);
		if (atv->att_val_type != ATT_VAL_TYPE_LIST) {
			if ((str = attrib_val_tostring(atv, B_FALSE)) != NULL) {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" value \"%s\" "
				    "should be in ()"), atname, str);
				free(str);
			} else {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" value "
				    "should be in ()"), atname);
			}
			ret = 1;
			continue;
		}
		/* Values should be in the form (priv, val, actions) */
		if (lst_size(atv->att_val_values) < 3) {
			util_add_errmsg(errlst, gettext(
			    "rctl \"%s\" value should be in the form "
			    "(priv, val, action[,...])[,....]"), atname);
			ret = 1;
			continue;
		}

		priv = lst_at(atv->att_val_values, 0);
		val = lst_at(atv->att_val_values, 1);
		/* actions = el[2], el[3], ... */

		vpriv = priv->att_val_value;
		rpriv = rule->resctl_privs;

		if (priv->att_val_type != ATT_VAL_TYPE_VALUE) {
			util_add_errmsg(errlst, gettext(
			    "rctl \"%s\" invalid privilege"), atname);
			ret = 1;
		} else if (!SIN3(vpriv, "basic", "privileged", "priv")) {
			util_add_errmsg(errlst, gettext(
			    "rctl \"%s\" unknown privilege \"%s\""),
			    atname, vpriv);
			ret = 1;
		} else if (!(
		    ((rpriv & RESCTL_PRIV_PRIVE) && SEQU(vpriv, "priv")) ||
		    ((rpriv & RESCTL_PRIV_PRIVD) && SEQU(vpriv, "privileged")) ||
		    ((rpriv & RESCTL_PRIV_BASIC) && SEQU(vpriv, "basic")))) {
			util_add_errmsg(errlst, gettext(
			    "rctl \"%s\" privilege not allowed \"%s\""),
			    atname, vpriv);
			ret = 1;
		}

		vval = val->att_val_value;
		rmax = rule->resctl_max;

		if (val->att_val_type != ATT_VAL_TYPE_VALUE) {
			util_add_errmsg(errlst, gettext(
			    "rctl \"%s\" invalid value"), atname);
			ret = 1;
		} else if (regexec(&pintexp, vval, 0, NULL, 0) != 0) {
			util_add_errmsg(errlst, gettext(
			    "rctl \"%s\" value \"%s\" is not an integer"),
			    atname, vval);
			ret = 1;
		} else if (strtoll(vval, NULL, 0) > rmax) {
			util_add_errmsg(errlst, gettext(
			    "rctl \"%s\" value \"%s\" exceeds system limit"),
			    atname, vval);
			ret = 1;
		}

		nonecount = 0;
		denycount = 0;
		sigcount = 0;

		for (j = 2; j < lst_size(atv->att_val_values); j++) {
			action = lst_at(atv->att_val_values,j);

			if (action->att_val_type != ATT_VAL_TYPE_VALUE) {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" invalid action"), atname);
				ret = 1;
				continue;
			}

			vaction = action->att_val_value;

			if (regexec(&sigacexp, vaction, 0, NULL, 0) != 0 &&
			    !SIN2(vaction, "none", "deny")) {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" unknown action \"%s\""),
				    atname, vaction);
				ret = 1;
				continue;
			}

			raction = rule->resctl_action;
			if (!(((raction & RESCTL_ACTN_SIGN) &&
			    regexec(&sigacexp, vaction, 0, NULL, 0) == 0) ||
			    ((raction & RESCTL_ACTN_NONE) &&
			    SEQU(vaction, "none")) ||
			    ((raction & RESCTL_ACTN_DENY) &&
			    SEQU(vaction, "deny")))) {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" action not allowed \"%s\""),
				    atname, vaction);
				ret = 1;
				continue;
			}

			if (SEQU(vaction, "none")) {
				if (nonecount >= 1) {
					util_add_errmsg(errlst, gettext(
					    "rctl \"%s\" duplicate action "
					    "none"), atname);
					ret = 1;
				}
				nonecount++;
				continue;
			}

			if (SEQU(vaction, "deny")) {
				if (denycount >= 1) {
					util_add_errmsg(errlst, gettext(
					    "rctl \"%s\" duplicate action "
					    "deny"), atname);
					ret = 1;
				}
				denycount++;
				continue;
			}

			/* At this point, the action must be signal. */
			if (sigcount >= 1) {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" duplicate action sig"),
				    atname);
				ret = 1;
			}
			sigcount++;

			/*
			 * Make sure signal is correct format, on of:
			 * sig=##
			 * signal=##
			 * sig=SIGXXX
			 * signal=SIGXXX
			 * sig=XXX
			 * signal=XXX
			 */

			if (regexec(&signlexp, vaction, nmatch, mat, 0) != 0 ||
			    (sigstr = util_substr(&signlexp, mat, vaction, 2))
			    == NULL) {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" invalid signal \"%s\""),
				    atname, vaction);
				ret = 1;
				continue;
			}

			/* Our version of sigstr =~ s/SIG// */
			if (strstr(sigstr, "SIG") != NULL)
				sigstr = strstr(sigstr, "SIG") + 3;

			sigval = 0;
			for (k = 0; k < SIGS_CNT; k++) {
				if (SEQU(sigs[k].sig, sigstr))
					sigval = sigs[k].mask;
			}
			free(sigstr);

			if (sigval == 0) {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" invalid signal \"%s\""),
				    atname, vaction);
				ret = 1;
				continue;
			}

			if (!(sigval & rule->resctl_sigs)) {
				util_add_errmsg(errlst, gettext(
				    "rctl \"%s\" signal not allowed \"%s\""),
				    atname, vaction);
				ret = 1;
				continue;
			}
		}
		if (nonecount > 0 && (denycount > 0 || sigcount > 0)) {
			util_add_errmsg(errlst, gettext(
			    "rctl \"%s\" action \"none\" specified with "
			    "other actions"),
			    atname);
			ret = 1;
		}
	}

out:
	free(mat);
	regfree(&signlexp);
	regfree(&sigacexp);
	regfree(&pintexp);
	return (ret);
}

int
attrib_validate(attrib_t *att, lst_t *errlst)
{
	int ret = 0;
	char *atname = att->att_name;
	attrib_val_t *atv = att->att_value;
	int atv_type = atv->att_val_type;
	char *str, *eptr;
	long long ll;

	resctl_info_t rinfo;
	resctlrule_t rrule;

	regex_t poolnexp;
	if (regcomp(&poolnexp, POOLN_EXP, REG_EXTENDED) != 0) {
			util_add_errmsg(errlst, gettext(
			    "Failed to compile poolname regular expression:"));
			return (1);
	}

	if (SEQU(atname, "task.final")) {
		if (atv_type != ATT_VAL_TYPE_NULL) {
			util_add_errmsg(errlst, gettext(
			    "task.final should not have value"));
			ret = 1;
		}
	} else if (SEQU(atname, "rcap.max-rss")) {
		if (atv_type == ATT_VAL_TYPE_NULL) {
			util_add_errmsg(errlst, gettext(
			    "rcap.max-rss missing value"));
			ret = 1;
		} else if (atv_type == ATT_VAL_TYPE_LIST) {
			util_add_errmsg(errlst, gettext(
			    "rcap.max-rss should have single value"));
			ret = 1;
		} else if (atv_type == ATT_VAL_TYPE_VALUE) {
			if ((str = attrib_val_tostring(atv, B_FALSE)) != NULL) {
				ll = strtoll(str, &eptr, 0);
				if (*eptr != '\0') {
					util_add_errmsg(errlst, gettext(
					    "rcap.max-rss is not an integer "
					    "value: \"%s\""), str);
					ret = 1;
				} else if (ll == LLONG_MIN && errno == ERANGE) {
					util_add_errmsg(errlst, gettext(
					    "rcap.max-rss too small"));
					ret = 1;
				} else if (ll == LLONG_MAX && errno == ERANGE) {
					util_add_errmsg(errlst, gettext(
					    "rcap.max-rss too large"));
					ret = 1;
				} else if (ll < 0) {
					util_add_errmsg(errlst, gettext(
					    "rcap.max-rss should not have "
					    "negative value: \"%s\""), str);
					ret = 1;
				}
				free(str);
			} else {
				util_add_errmsg(errlst, gettext(
				    "rcap.max-rss has invalid value"));
				ret = 1;
			}
		}
	} else if (SEQU(atname, "project.pool")) {
		if (atv_type == ATT_VAL_TYPE_NULL) {
			util_add_errmsg(errlst, gettext(
			    "project.pool missing value"));
			ret = 1;
		} else if (atv_type == ATT_VAL_TYPE_LIST) {
			util_add_errmsg(errlst, gettext(
			    "project.pool should have single value"));
			ret = 1;
		} else if (atv_type == ATT_VAL_TYPE_VALUE) {
			if ((str = attrib_val_tostring(atv, B_FALSE)) != NULL) {
				if (regexec(&poolnexp, str, 0, NULL, 0) != 0) {
					util_add_errmsg(errlst, gettext(
					    "project.pool: invalid pool "
					    "name \"%s\""), str);
					ret = 1;
				} else if (resctl_pool_exist(str) != 0) {
					util_add_errmsg(errlst, gettext(
					    "project.pool: pools not enabled  "
					    "or pool does not exist: \"%s\""),
					    str);
					ret = 1;
				}
				free(str);
			} else {
				util_add_errmsg(errlst, gettext(
				    "project.pool has invalid value "));
				ret = 1;
			}
		}
	} else if (resctl_get_info(atname, &rinfo) == 0) {
		resctl_get_rule(&rinfo, &rrule);
		if (attrib_validate_rctl(att, &rrule, errlst) != 0) {
			ret = 1;
		}
	}

	regfree(&poolnexp);
	return (ret);
}

int
attrib_validate_lst(lst_t *attribs, lst_t *errlst)
{
	int i, j;
	attrib_t *att;
	char **atnames, **atlast;
	char *atname;
	int ret = 0;

	atlast = atnames = util_safe_zmalloc(
	    (lst_size(attribs) + 1) * sizeof(char *));
	for (i = 0; i < lst_size(attribs); i++) {
		att = lst_at(attribs, i);

		/* Validate this attribute */
		if(attrib_validate(att, errlst) != 0)
			ret = 1;
		/* Make sure it is not duplicated */
		for (j = 0; (atname = atnames[j]) != NULL; j++) {
			if (strcmp(atname, att->att_name) == 0) {
				util_add_errmsg(errlst, gettext(
				    "Duplicate attributes \"%s\""), atname);
				ret = 1;
			}
		}
		/*
		 * Add it to the attribute name to the
		 * temporary list if not found
		 */
		if (atname == NULL) {
			*atlast++ = att->att_name;
		}
	}
	free(atnames);
	return (ret);
}

attrib_t
*attrib_alloc()
{
	return (util_safe_zmalloc(sizeof(attrib_t)));
}

attrib_val_t
*attrib_val_alloc(int type, void *val)
{
	attrib_val_t *ret;

	ret = util_safe_malloc(sizeof(attrib_val_t));
	ret->att_val_type = type;
	ret->att_val_value = val;
	return (ret);
}

char
*attrib_val_tostring(attrib_val_t *val, boolean_t innerlist)
{
	char *ret = NULL;
	char *vstring;
	int i;
	attrib_val_t *v;
	switch(val->att_val_type) {
		case ATT_VAL_TYPE_NULL:
			return util_safe_strdup("");
		case ATT_VAL_TYPE_VALUE:
			return util_safe_strdup(val->att_val_value);
		case ATT_VAL_TYPE_LIST:
			/* Only innerlists need to be betweeen ( and ) */
			if (innerlist)
				ret = UTIL_STR_APPEND1(ret, "(");
			for (i = 0; i < lst_size(val->att_val_values);
			    i++) {
				v = lst_at(val->att_val_values, i);
				if (i > 0) {
					ret = UTIL_STR_APPEND1(ret, ",");
				}
				if ((vstring =
				    attrib_val_tostring(v, B_TRUE)) == NULL) {
					UTIL_FREE_SNULL(ret);
					goto out;
				}
				ret = UTIL_STR_APPEND1(ret, vstring);
				free(vstring);
			}
			if (innerlist)
				ret = UTIL_STR_APPEND1(ret, ")");
			return (ret);
	}

out:
	return (ret);
}

char
*attrib_tostring(void *at)
{
	attrib_t *att;
	char *ret = NULL, *vstring;

	att = (attrib_t *)at;
	ret = UTIL_STR_APPEND1(ret, att->att_name);
	if ((vstring = attrib_val_tostring(att->att_value, B_FALSE)) != NULL) {
		if (strlen(vstring) > 0)
			ret = UTIL_STR_APPEND2(ret, "=", vstring);
		free(vstring);
		return (ret);
	}
	UTIL_FREE_SNULL(ret);
	return (ret);
}

char
*attrib_lst_tostring(lst_t *attrs)
{
	int i;
	attrib_t *att;
	char *ret = NULL;
	char *str;

	ret = UTIL_STR_APPEND1(ret, "");
	for (i = 0; i < lst_size(attrs); i++) {
		att = lst_at(attrs, i);

		if ((str = attrib_tostring(att)) != NULL) {
			if (i > 0)
				ret = UTIL_STR_APPEND1(ret, ";");

			ret = UTIL_STR_APPEND1(ret, str);
			free(str);
			continue;
		}

		free(ret);
		return (NULL);
	}
	return (ret);
}

void
attrib_val_free(attrib_val_t *atv)
{
	attrib_val_t *val;

	if (atv->att_val_type == ATT_VAL_TYPE_VALUE) {
		free(atv->att_val_value);
	} else if (atv->att_val_type == ATT_VAL_TYPE_LIST) {
		while (!lst_is_empty(atv->att_val_values)) {
			val = lst_at(atv->att_val_values, 0);
			(void) lst_remove(atv->att_val_values, val);
			attrib_val_free(val);
			free(val);
		}
		free(atv->att_val_values);
	}
}

void
attrib_free(attrib_t *att)
{
	free(att->att_name);
	if (att->att_value != NULL) {
		attrib_val_free(att->att_value);
		free(att->att_value);
	}
}
void
attrib_free_lst(lst_t *attribs)
{
	attrib_t *att;

	if (attribs == NULL)
		return;

	while (!lst_is_empty(attribs)) {
		att = lst_at(attribs, 0);
		(void) lst_remove(attribs, att);
		attrib_free(att);
		free(att);
	}
}

void
attrib_sort_lst(lst_t *attribs)
{
	int i, j, n;
	attrib_t *atti, *attj;

	if (attribs == NULL)
		return;

	n = lst_size(attribs);
	for (i = 0; i < n - 1; i++) {
		for (j = i + 1; j < n; j++) {
			atti = lst_at(attribs, i);
			attj = lst_at(attribs, j);
			if (strcmp(attj->att_name, atti->att_name) < 0) {
				(void) lst_replace_at(attribs, i, attj);
				(void) lst_replace_at(attribs, j, atti);
			}
		}
	}
}

void
attrib_val_to_list(attrib_val_t *atv)
{
	void *val;
	int type;
	attrib_val_t *mat;

	if (atv->att_val_type == ATT_VAL_TYPE_LIST)
		return;

	val = atv->att_val_value;
	type = atv->att_val_type;

	atv->att_val_type = ATT_VAL_TYPE_LIST;
	atv->att_val_values = util_safe_malloc(sizeof(lst_t));
	lst_create(atv->att_val_values);

	if (type == ATT_VAL_TYPE_VALUE && val != NULL) {
		mat = ATT_VAL_ALLOC_VALUE(val);
		lst_insert_tail(atv->att_val_values, mat);
	}
}

void
attrib_val_append(attrib_val_t *atv, char *token)
{
	attrib_val_t *nat;
	if (atv->att_val_type == ATT_VAL_TYPE_VALUE) {
		/* convert this to LIST attribute */
		attrib_val_to_list(atv);
	}

	if (atv->att_val_type == ATT_VAL_TYPE_NULL) {
		/* convert this to VALUE attribute */
		atv->att_val_type = ATT_VAL_TYPE_VALUE;
		atv->att_val_value = util_safe_strdup(token);
	} else if (atv->att_val_type == ATT_VAL_TYPE_LIST) {
		/* append token to the list */
		nat = ATT_VAL_ALLOC_VALUE(util_safe_strdup(token));
		lst_insert_tail(atv->att_val_values, nat);
	}
}

attrib_val_t
*attrib_val_parse(char *values, lst_t *errlst)
{
	attrib_val_t *ret = NULL;
	attrib_val_t *at;
	attrib_val_t *nat;
	lst_t stk;

	char **tokens, *token, *usedtokens, *prev;
	int i, error, parendepth;

	error = parendepth = 0;
	prev = "";

	if ((tokens = util_tokenize(values, errlst)) == NULL) {
		goto out1;
	}

	lst_create(&stk);
	usedtokens = UTIL_STR_APPEND1(NULL, "");

	at = ret = ATT_VAL_ALLOC_NULL();

	for (i = 0; (token = tokens[i]) != NULL; i++) {

		usedtokens = UTIL_STR_APPEND1(usedtokens, token);

		if (SEQU(token, ",")) {
			if (SIN3(prev, "," , "(", "")) {
				attrib_val_append(at, "");
			}
			attrib_val_to_list(at);
			prev = ",";
		} else if (SEQU(token, "(")) {
			if (!(SIN3(prev, "(", ",", ""))) {
				util_add_errmsg(errlst, gettext(
				    "\"%s\" <- \"(\" unexpected"), usedtokens); 
				error = 1;
				goto out;
			}

			switch(at->att_val_type) {
				case ATT_VAL_TYPE_VALUE:
					util_add_errmsg(errlst, gettext(
					    "\"%s\" <- \"%s\" unexpected"),
					    usedtokens, token); 
					error = 1;
					goto out;
				case ATT_VAL_TYPE_NULL:
					/* Make is a LIST attrib */
					attrib_val_to_list(at);
					/*break; */
					/*FALLTHROUGH*/
				case ATT_VAL_TYPE_LIST:
					/* Allocate NULL node */
					nat = ATT_VAL_ALLOC_NULL();
					attrib_val_to_list(nat);
					lst_insert_tail(
					    at->att_val_values, nat);
					/* push at down one level */
					lst_insert_head(&stk, at);
					at = nat;
				break;
			}
			parendepth++;
			prev = "(";
		} else if (SEQU(token, ")")) {
			if (parendepth <= 0) {
				util_add_errmsg(errlst, gettext(
				    "\"%s\" <- \")\" unexpected"), usedtokens); 
				error = 1;
				goto out;
			}
			if (SIN2(prev, ",", "(")) {
				attrib_val_append(at, "");
			}

			if (!lst_is_empty(&stk)) {
				at = lst_at(&stk, 0);
				(void) lst_remove(&stk, at);
			}
			parendepth--;
			prev = ")";
		} else {
			if (!(SIN3(prev, ",", "(", ""))) {
				util_add_errmsg(errlst, gettext(
				    "\"%s\" <- \"%s\" unexpected"),
				    usedtokens, token); 
				error = 1;
				goto out;
			}

			attrib_val_append(at, token);
			prev = token;
		}
	}

	if (parendepth != 0) {
		util_add_errmsg(errlst, gettext(
		    "\"%s\" <- \")\" missing"),
		    usedtokens); 
		error = 1;
		goto out;
	}

	if (SIN2(prev, ",", "")) {
		switch(at->att_val_type) {
			case ATT_VAL_TYPE_NULL:
				util_add_errmsg(errlst, gettext(
				    "\"%s\" unexpected"),
				    usedtokens); 
				error = 1;
				goto out;
			case ATT_VAL_TYPE_VALUE:
			case ATT_VAL_TYPE_LIST:
				attrib_val_append(at, "");
			break;
		}
	}

out:
	while (!lst_is_empty(&stk)) {
                at = lst_at(&stk, 0);
                (void) lst_remove(&stk, at);
        }

	util_free_tokens(tokens);
	free(tokens);
	free(usedtokens);
	if (error) {
		attrib_val_free(ret);
		UTIL_FREE_SNULL(ret);
	}
out1:
	return (ret);
}

attrib_t
*attrib_parse(regex_t *attrbexp, regex_t *atvalexp, char *att, int flags,
    lst_t *errlst)
{
	int nmatch = MAX_OF(attrbexp->re_nsub, atvalexp->re_nsub) + 1;
	attrib_t *ret = NULL;
	attrib_val_t *retv, *atv, *atvl;
	char *values = NULL;
	int vidx, nidx, vlen;
	int scale;

	char *num, *mod, *unit;
	int i;

	resctl_info_t rinfo;
	resctlrule_t rrule;

	regmatch_t *mat = util_safe_malloc(nmatch * sizeof(regmatch_t));
	ret = ATT_ALLOC();

	if (regexec(attrbexp, att, attrbexp->re_nsub + 1 , mat, 0) == 0) {
		ret->att_name = util_safe_strdup(att);
		ret->att_value = ATT_VAL_ALLOC_NULL();
	} else if (regexec(atvalexp, att,
	    atvalexp->re_nsub + 1, mat, 0) == 0) {
		vidx = atvalexp->re_nsub;
		vlen = mat[vidx].rm_eo - mat[vidx].rm_so;
		nidx = atvalexp->re_nsub - 3;
		ret->att_name = util_substr(atvalexp, mat, att, nidx); 

		if (vlen > 0) {
			values = util_substr(atvalexp, mat, att, vidx);
			ret->att_value = attrib_val_parse(values, errlst);
			free(values);
			if (ret->att_value == NULL) {
				util_add_errmsg(errlst, gettext(
				    "Invalid value on attribute \"%s\""),
				    ret->att_name);
				attrib_free(ret);
				UTIL_FREE_SNULL(ret);
				goto out;
			}
		} else {
			/* the value is an empty string */
			ret->att_value = ATT_VAL_ALLOC_NULL();
		}
	} else {
		util_add_errmsg(errlst, gettext(
		    "Invalid attribute \"%s\""), att);
		attrib_free(ret);
		UTIL_FREE_SNULL(ret);
		goto out;
	}

	if (!(flags & F_PAR_UNT))
		goto out;

	if (SEQU(ret->att_name, "rcap.max-rss")) {
		values = attrib_val_tostring(ret->att_value, B_FALSE);
		if (util_val2num(values, BYTES_SCALE, errlst,
		    &num, &mod, &unit) == 0) {
			attrib_val_free(ret->att_value);
			ret->att_value = ATT_VAL_ALLOC_VALUE(num);
			free(mod);
			free(unit);
		} else {
			attrib_free(ret);
			UTIL_FREE_SNULL(ret);
			goto out;
		}
		free(values);
	}

	if (resctl_get_info(ret->att_name, &rinfo) == 0) {
		resctl_get_rule(&rinfo, &rrule);
		retv = ret->att_value;

		/*
		 * Handle the case of RESCTL_TYPE_SCNDS
		 * and RESCTL_TYPE_BYTES only
		 */
		if (!(rrule.resctl_type == RESCTL_TYPE_SCNDS ||
		    rrule.resctl_type == RESCTL_TYPE_BYTES))
			goto out;

		scale = (rrule.resctl_type == RESCTL_TYPE_SCNDS)?
		    SCNDS_SCALE : BYTES_SCALE;

		if (retv->att_val_type != ATT_VAL_TYPE_LIST)
			goto out;


		for (i = 0; i < lst_size(retv->att_val_values); i++) {
			atvl = atv = lst_at(retv->att_val_values, i);

			/*
			 * Continue if not a list and the second value
			 * is not a scaler value
			 */
			if (atv->att_val_type != ATT_VAL_TYPE_LIST ||
			    lst_size(atv->att_val_values) < 3 ||
			    (atv = lst_at(atv->att_val_values, 1)) == NULL ||
			    atv->att_val_type != ATT_VAL_TYPE_VALUE) {
				continue;
			}
			values = attrib_val_tostring(atv, B_FALSE);
			if (util_val2num(values, scale, errlst,
			    &num, &mod, &unit) == 0) {
				attrib_val_free(atv);
				atv = ATT_VAL_ALLOC_VALUE(num);
				(void) lst_replace_at(atvl->att_val_values, 1,
				    atv);
				free(mod);
				free(unit);

			} else {
				free(values);
				attrib_free(ret);
				UTIL_FREE_SNULL(ret);
				goto out;
			}
			free(values);
		}
	}

out:
	free(mat);
	return (ret);
}

lst_t
*attrib_parse_attributes(char *attribs, int flags, lst_t *errlst)
{
	char *sattrs, *attrs, *att;
	regex_t attrbexp, atvalexp;

	attrib_t *natt = NULL;
	lst_t *ret = NULL;

	ret = util_safe_malloc(sizeof(lst_t));
	lst_create(ret);

	if (regcomp(&attrbexp, ATTRB_EXP, REG_EXTENDED) != 0)
		goto out1;
	if (regcomp(&atvalexp, ATVAL_EXP, REG_EXTENDED) != 0)
		goto out2;

	sattrs = attrs = util_safe_strdup(attribs);
	while ((att = strsep(&attrs, ";")) != NULL) {
		if (*att == '\0')
			continue;
		if ((natt = attrib_parse(&attrbexp,
		    &atvalexp, att, flags, errlst)) == NULL) {
			attrib_free_lst(ret);
			UTIL_FREE_SNULL(ret);
			break;
		}

		lst_insert_tail(ret, natt);
	}

	free(sattrs);
	regfree(&atvalexp);
out2:
	regfree(&attrbexp);
out1:
	return (ret);
}

attrib_val_t
*attrib_val_duplicate(attrib_val_t *atv) {
	int i;
	lst_t *values;
	attrib_val_t *val;
	attrib_val_t *natv;

	switch(atv->att_val_type) {
		case ATT_VAL_TYPE_NULL:
			natv = ATT_VAL_ALLOC_NULL();
		break;
		case ATT_VAL_TYPE_VALUE:
			natv = ATT_VAL_ALLOC_VALUE(util_safe_strdup(atv->att_val_value));
		break;
		case ATT_VAL_TYPE_LIST:
			values = util_safe_malloc(sizeof(lst_t));
			lst_create(values);
			for (i = 0; i < lst_size(atv->att_val_values);
			    i++) {
				val = lst_at(atv->att_val_values, i);
				lst_insert_tail(values,
				    attrib_val_duplicate(val));
			}
			natv = ATT_VAL_ALLOC_LIST(values);
		break;
	}

	return natv;
}

attrib_t
*attrib_duplicate(attrib_t *att) {
	attrib_t *natt = ATT_ALLOC();
	natt->att_name = util_safe_strdup(att->att_name);
	natt->att_value = attrib_val_duplicate(att->att_value);
	return (natt);
}

attrib_t
*attrib_merge_add(attrib_t *eatt, attrib_t *natt)
{
	int i;
	attrib_t *att;
	attrib_val_t *atv, *eatv, *natv;
	lst_t *values;

	eatv = eatt->att_value;
	natv = natt->att_value;
	att = ATT_ALLOC();
	att->att_name = util_safe_strdup(eatt->att_name);

	if (eatv->att_val_type == ATT_VAL_TYPE_NULL) {

		/* NULL + X -> X */
		atv = attrib_val_duplicate(natv);

	} else if (natv->att_val_type == ATT_VAL_TYPE_NULL) {

		/* X + NULL -> X */
		atv = attrib_val_duplicate(eatv);

	} else if (eatv->att_val_type == ATT_VAL_TYPE_VALUE &&
	    natv->att_val_type == ATT_VAL_TYPE_VALUE) {

		/* VALUE + VALUE -> LIST */
		values = util_safe_malloc(sizeof(lst_t));
		lst_create(values);
		lst_insert_tail(values, attrib_val_duplicate(eatv));
		lst_insert_tail(values, attrib_val_duplicate(natv));
		atv = ATT_VAL_ALLOC_LIST(values);

	} else if (eatv->att_val_type == ATT_VAL_TYPE_VALUE &&
	    natv->att_val_type == ATT_VAL_TYPE_LIST) {

		/* VALUE + LIST -> LIST */
		atv = attrib_val_duplicate(natv);
		lst_insert_head(atv->att_val_values,
		    attrib_val_duplicate(eatv));

	} else if (eatv->att_val_type == ATT_VAL_TYPE_LIST &&
	    natv->att_val_type == ATT_VAL_TYPE_VALUE) {

		/* LIST + VALUE -> LIST */
		atv = attrib_val_duplicate(eatv);
		lst_insert_tail(atv->att_val_values,
		    attrib_val_duplicate(natv));

	} else if (eatv->att_val_type == ATT_VAL_TYPE_LIST &&
	    natv->att_val_type == ATT_VAL_TYPE_LIST) {

		/* LIST + LIST -> LIST */
		atv = attrib_val_duplicate(eatv);
		for (i = 0; i < lst_size(natv->att_val_values); i++) {
			lst_insert_tail(atv->att_val_values,
			    attrib_val_duplicate(
			    lst_at(natv->att_val_values,i)));
		}
	}

	att->att_value = atv;
	return (att);
}

int
attrib_val_equal(attrib_val_t *xatv,attrib_val_t *yatv)
{
	int i;
	attrib_val_t *xv, *yv;

	if (xatv->att_val_type != yatv->att_val_type)
		return (1);

	switch (xatv->att_val_type) {
		case ATT_VAL_TYPE_NULL:
			return (0);
		case ATT_VAL_TYPE_VALUE:
			if (SEQU(xatv->att_val_value, yatv->att_val_value)) {
				return (0);
			}
			return (1);
		case ATT_VAL_TYPE_LIST:
			if (lst_size(xatv->att_val_values) !=
			    lst_size(yatv->att_val_values))
				return (1);
			for (i = 0; i < lst_size(xatv->att_val_values);
			    i++) {
				xv = lst_at(xatv->att_val_values, i);
				yv = lst_at(yatv->att_val_values, i);
				if (attrib_val_equal(xv, yv) != 0) {
					return (1);
				}
			}
		break;
	}
	return (0);
}

attrib_t
*attrib_merge_remove(attrib_t *eatt, attrib_t *natt, lst_t *errlst)
{

	int i, j;
	attrib_t *att = NULL;
	attrib_val_t *eatv, *natv;
	attrib_val_t *ev, *nv1, *nv2;
	lst_t *values;
	boolean_t found;

	eatv = eatt->att_value;
	natv = natt->att_value;

	if (eatv->att_val_type == ATT_VAL_TYPE_NULL &&
	    natv->att_val_type == ATT_VAL_TYPE_NULL) {

		/* NULL - NULL -> EMPTY */
		att = attrib_duplicate(eatt);
		(void) strcpy(att->att_name, "");

	} else if (eatv->att_val_type == ATT_VAL_TYPE_NULL ||
	    (eatv->att_val_type == ATT_VAL_TYPE_VALUE &&
	    natv->att_val_type == ATT_VAL_TYPE_LIST) ||
	    (eatv->att_val_type == ATT_VAL_TYPE_LIST &&
	    natv->att_val_type == ATT_VAL_TYPE_VALUE)) {

		/* NULL - X -> ERR, VALUE - LIST -> ERR, LIST - VALUE -> ERR */
		util_add_errmsg(errlst, gettext(
		    "Can not remove attribute \"%s\""),
		    eatt->att_name);

	} else if (natv->att_val_type == ATT_VAL_TYPE_NULL) {

		/* X - NULL -> X */
		att = attrib_duplicate(eatt);

	} else if (eatv->att_val_type == ATT_VAL_TYPE_VALUE &&
	    natv->att_val_type == ATT_VAL_TYPE_VALUE) {

		/* VALUE - VALUE -> {EMPTY | ERR} */
		if (attrib_val_equal(eatv, natv) == 0) {
			att = ATT_ALLOC();
			att->att_name = util_safe_strdup("");
			att->att_value = ATT_VAL_ALLOC_NULL();
		} else {
			util_add_errmsg(errlst, gettext(
			    "Can not remove attribute \"%s\""),
			    eatt->att_name);
		}

	} else if (eatv->att_val_type == ATT_VAL_TYPE_LIST &&
	    natv->att_val_type == ATT_VAL_TYPE_LIST) {
		/* LIST - LIST -> {EMPTY | ERR | LIST} */
		if (attrib_val_equal(eatv, natv) == 0) {
			att = ATT_ALLOC();
			att->att_name = util_safe_strdup("");
			att->att_value = ATT_VAL_ALLOC_NULL();
			goto out;
		}

		for (i = 0; i < lst_size(natv->att_val_values); i++) {
			nv1 = lst_at(natv->att_val_values, i);
			for (j = 0; j < lst_size(natv->att_val_values);
			    j++) {
				nv2 = lst_at(natv->att_val_values, j);
				if (i != j && attrib_val_equal(nv1, nv2) == 0) {
					util_add_errmsg(errlst, gettext(
					    "Duplicate values, can not remove"
					    " attribute \"%s\""),
					    eatt->att_name);
					goto out;
				}
			}

			found = B_FALSE;
			for (j = 0; j < lst_size(eatv->att_val_values);
			    j++) {
				ev = lst_at(eatv->att_val_values, j);
				if (attrib_val_equal(nv1, ev) == 0) {
					found = B_TRUE;
					break;
				}
			}

			if (!found) {
				util_add_errmsg(errlst, gettext(
				    "Value not found, can not remove"
				    " attribute \"%s\""),
				    eatt->att_name);
				goto out;
			}
		}

		values = util_safe_malloc(sizeof(lst_t));
		lst_create(values);
		for (i = 0; i < lst_size(eatv->att_val_values); i++) {
			ev = lst_at(eatv->att_val_values, i);
			found = B_FALSE;
			for (j = 0; j < lst_size(natv->att_val_values);
			    j++) {
				nv1 = lst_at(natv->att_val_values, j);
				if (attrib_val_equal(ev, nv1) == 0) {
					found = B_TRUE;
					break;
				}
			}

			if (!found) {
				lst_insert_tail(values,
				    attrib_val_duplicate(ev));
			}
		}
		att = ATT_ALLOC();
		att->att_name = util_safe_strdup(eatt->att_name);
		att->att_value = ATT_VAL_ALLOC_LIST(values);
	}

out:
	return (att);
}

attrib_t
*attrib_merge(attrib_t *eatt, attrib_t *natt, int flags, lst_t *errlst)
{
	attrib_t *att = NULL;

	VERIFY(SEQU(eatt->att_name, natt->att_name));

	if (flags & F_MOD_ADD) {
		att = attrib_merge_add(eatt, natt);
	} else if (flags & F_MOD_REM) {
		att = attrib_merge_remove(eatt, natt, errlst);
	}

	return att;
}

void
attrib_merge_attrib_lst(lst_t **eattrs, lst_t *nattrs, int flags,
    lst_t *errlst) {

	lst_t* attrs = NULL;
	int i, j;
	attrib_t *att, *natt, *eatt;
	boolean_t found;

	if (flags & F_MOD_ADD) {
		attrs = util_safe_malloc(sizeof(lst_t));
		lst_create(attrs);

		for (i = 0; i < lst_size(*eattrs); i++) {
			eatt = lst_at(*eattrs, i);
			found = B_FALSE;
			for (j = 0; j < lst_size(nattrs); j++) {
				natt = lst_at(nattrs, j);
				if (SEQU(eatt->att_name, natt->att_name)) {
					found = B_TRUE;
					break;
				}
			}

			att = found ? attrib_merge(eatt, natt, flags, errlst) :
			    attrib_duplicate(eatt);
			if (att == NULL) {
				attrib_free_lst(attrs);
				UTIL_FREE_SNULL(attrs);
				goto out;
			}
			lst_insert_tail(attrs, att);
		}

		for (i = 0; i < lst_size(nattrs); i++) {
			natt = lst_at(nattrs, i);
			found = B_FALSE;
			for (j = 0; j < lst_size(*eattrs); j++) {
				eatt = lst_at(*eattrs, j);
				if (SEQU(natt->att_name, eatt->att_name)) {
					found = B_TRUE;
					break;
				}
			}
			if (found)
				continue;
			lst_insert_tail(attrs,attrib_duplicate(natt));
		}

	} else if (flags & (F_MOD_REM | F_MOD_SUB)) {

		for (i = 0; i < lst_size(nattrs); i++) {
			natt = lst_at(nattrs, i);
			for (j = 0; j < lst_size(nattrs); j++) {
				att = lst_at(nattrs, j);
				if (SEQU(natt->att_name, att->att_name) &&
				    i != j) {
					util_add_errmsg(errlst, gettext(
					    "Duplicate Attributes \"%s\""),
					    natt->att_name);
					goto out;
				}
			}

			found = B_FALSE;
			for (j = 0; j < lst_size(*eattrs); j++) {
				eatt = lst_at(*eattrs, j);
				if (SEQU(eatt->att_name, natt->att_name)) {
					found = B_TRUE;
					break;
				}
			}
			if (!found) {
				util_add_errmsg(errlst, gettext(
				    "Project does not contain \"%s\""),
				    natt->att_name);
				goto out;
			}
		}

		attrs = util_safe_malloc(sizeof(lst_t));
		lst_create(attrs);

		for (i = 0; i < lst_size(*eattrs); i++) {
			eatt = lst_at(*eattrs, i);
			found = B_FALSE;
			for (j = 0; j < lst_size(nattrs); j++) {
				natt = lst_at(nattrs, j);
				if (SEQU(eatt->att_name, natt->att_name)) {
					found = B_TRUE;
					break;
				}
			}

			if (flags & F_MOD_REM) {
				att = found ?
				    attrib_merge(eatt, natt, flags, errlst) :
				    attrib_duplicate(eatt);
			} else if (flags & F_MOD_SUB) {
				att = attrib_duplicate(found ? natt : eatt);
			}

			if (att == NULL) {
				attrib_free_lst(attrs);
				UTIL_FREE_SNULL(attrs);
				goto out;
			} else if (SEQU(att->att_name,"")) {
				attrib_free(att);
			} else {
				lst_insert_tail(attrs, att);
			}
		}

	} else if (flags & F_MOD_REP) {
		attrs = util_safe_malloc(sizeof(lst_t));
		lst_create(attrs);
		for (i = 0; i < lst_size(nattrs); i++) {
			natt = lst_at(nattrs, i);
			lst_insert_tail(attrs, attrib_duplicate(natt));
		}
	}
out:
	if (attrs != NULL) {
		attrib_free_lst(*eattrs);
		free(*eattrs);
		*eattrs = attrs;
	}
}
