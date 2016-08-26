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

#include "attrib.h"
#include "rctl.h"
#include "util.h"

#define MAX_OF(X,Y)	(((X) > (Y)) ? (X) : (Y))


#define SEQU(X,Y)		(strcmp((X), (Y)) == 0)
#define	SIN1(X, S1)		((SEQU((X), (S1))))
#define	SIN2(X, S1, S2)		((SEQU((X), (S1))) || (SIN1((X), (S2))))
#define	SIN3(X, S1, S2, S3)	((SEQU((X), (S1))) || (SIN2((X), (S2), (S3))))


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
*attrib_val_tostring(attrib_val_t *val)
{
	char *ret = NULL;
	char *vstring;
	int i;
	attrib_val_t *v;
	switch(val->att_val_type) {
		case ATT_VAL_TYPE_NULL:
			return strdup("");
		break;
		case ATT_VAL_TYPE_VALUE:
			return strdup(val->att_val_value);
		break;
		case ATT_VAL_TYPE_LIST:
			ret = UTIL_STR_APPEND1(ret, "(");
			for (i = 0, v = lst_at(val->att_val_values, 0);
			    v != NULL;
			    i++, v = lst_at(val->att_val_values, i)) {
				if (i > 0) {
					ret = UTIL_STR_APPEND1(ret, ",");
				}
				if ((vstring =
				    attrib_val_tostring(v)) == NULL) {
					free(ret);
					ret = NULL;
					goto out;
				}
				ret = UTIL_STR_APPEND1(ret, vstring);
				free(vstring);
			}
			ret = UTIL_STR_APPEND1(ret, ")");
			return (ret);
		break;
	}

out:
	return (ret);
}

char
*attrib_tostring(attrib_t *att)
{
	char *ret = NULL, *vstring;

	ret = UTIL_STR_APPEND1(ret, att->att_name);
	if ((vstring = attrib_val_tostring(att->att_value)) != NULL) {
		if (strlen(vstring) > 0)
			ret = UTIL_STR_APPEND2(ret, "=", vstring);
		free(vstring);
		return (ret);
	}
	free(ret);
	ret = NULL;
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
	for (i = 0, att = lst_at(attrs, 0); att != NULL;
	    i++, att = lst_at(attrs, i)) {

		if ((str = attrib_tostring(att)) != NULL) {
			if (i > 0)
				ret = UTIL_STR_APPEND1(ret, ";");

			ret = UTIL_STR_APPEND1(ret, str);
			free(str);
			continue;
		}

		free(ret);
		ret = NULL;
		return (ret);
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
			lst_remove(atv->att_val_values, val);
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
	while (!lst_is_empty(attribs)) {
		att = lst_at(attribs, 0);
		lst_remove(attribs, att);
		attrib_free(att);
		free(att);
	}
}

int
attrib_val_to_list(attrib_val_t *atv)
{
	void *val;
	int type;
	attrib_val_t *mat;

	if (atv->att_val_type == ATT_VAL_TYPE_LIST)
		return (0);

	val = atv->att_val_value;
	type = atv->att_val_type;

	atv->att_val_type = ATT_VAL_TYPE_LIST;
	atv->att_val_values = util_safe_malloc(sizeof(lst_t));
	lst_create(atv->att_val_values);

	if (type == ATT_VAL_TYPE_VALUE && val != NULL) {
		mat = ATT_VAL_ALLOC_VALUE(val);
		lst_insert_tail(atv->att_val_values, mat);
	}
	return (0);
}

int
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
		atv->att_val_value = strdup(token);
	} else if (atv->att_val_type == ATT_VAL_TYPE_LIST) {
		/* append token to the list */
		nat = ATT_VAL_ALLOC_VALUE(strdup(token));
		lst_insert_tail(atv->att_val_values, nat);
	}

	return (0);
}

attrib_val_t
*attrib_val_parse(char *values, list_t *errlst)
{
	char **tokens = NULL;
	char *token;
	int i;

	attrib_val_t *ret = NULL;
	attrib_val_t *at;
	attrib_val_t *nat;
	attrib_val_t *mat;
	attrib_val_t *oat;
	lst_t stk;
	char *usedtokens = NULL;
	int error = 0;


	char *prev = "";
	int parendepth = 0;

	if ((tokens =
	    util_tokenize(values, errlst)) == NULL) {
		goto out1;
	}

	ret = ATT_VAL_ALLOC_NULL();

	at = ret;

	lst_create(&stk);
	usedtokens = UTIL_STR_APPEND1(usedtokens, "");


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
				break;
				case ATT_VAL_TYPE_NULL:
					/* Make is a LIST attrib */
					attrib_val_to_list(at);
					break;
				case ATT_VAL_TYPE_LIST:
					/* Allocate NULL node */
					nat = ATT_VAL_ALLOC_NULL();
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
				lst_remove(&stk, at);
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
			break;
			case ATT_VAL_TYPE_VALUE:
			case ATT_VAL_TYPE_LIST:
				attrib_val_append(at, "");
			break;
		}
	}

out:
	while (!lst_is_empty(&stk)) {
                at = lst_at(&stk, 0);
                lst_remove(&stk, at);
        }

	util_free_tokens(tokens);
	free(tokens);
	free(usedtokens);
	if (error) {
		attrib_val_free(ret);
		free(ret);
		ret = NULL;
	}
out1:
	return (ret);
}

attrib_t
*attrib_parse(regex_t *attrbexp, regex_t *atvalexp, char *att,
    list_t *errlst)
{
	int nmatch = MAX_OF(attrbexp->re_nsub, atvalexp->re_nsub) + 1;
	attrib_t *ret = NULL;
	attrib_val_t *retv, *atv, *atvl;
	char *values = NULL;
	int vstart, vend, vlen;
	int nstart, nend, nlen;
	int retlen;
	int vidx, nidx;
	int scale;

	char *num, *mod, *unit;
	int i;


	rctl_info_t rinfo;
	rctlrule_t rrule;

	regmatch_t *mat = util_safe_malloc(nmatch * sizeof(regmatch_t));
	ret = ATT_ALLOC();

	if (regexec(attrbexp, att, attrbexp->re_nsub + 1 , mat, 0) == 0) {
		ret->att_name = strdup(att);
		ret->att_value = ATT_VAL_ALLOC_NULL();
	} else if (regexec(atvalexp, att, atvalexp->re_nsub + 1, mat, 0) == 0) {
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
				free(ret);
				ret = NULL;
				goto out;
			}
		} else {
			/* the value is an empty string */
			ret->att_value = ATT_VAL_ALLOC_NULL();
		}
	} else {
		util_add_errmsg(errlst, gettext(
		    "Invalid attribute \"%s\""), att);
		goto out;
	}

	if (SEQU(ret->att_name, "rcap.max-rss")) {
		values = attrib_val_tostring(ret->att_value);
		if (util_val2num(values, BYTES_SCALE, errlst,
		    &num, &mod, &unit) == 0) {
			attrib_val_free(ret->att_value);
			ret->att_value = ATT_VAL_ALLOC_VALUE(num);
			free(mod);
			free(unit);
		} else {
			attrib_free(ret);
			free(ret);
			ret = NULL;
			goto out;
		}
		free(values);
	}

	if (rctl_get_info(ret->att_name, &rinfo) == 0) {
		rctl_get_rule(&rinfo, &rrule);
		retv = ret->att_value;

		/*
		 * Handle the case of RCTL_TYPE_SCNDS
		 * and RCTL_TYPE_BYTES only
		 */
		if (!(rrule.rtcl_type == RCTL_TYPE_SCNDS ||
		    rrule.rtcl_type == RCTL_TYPE_BYTES))
			goto out;

		scale = (rrule.rtcl_type == RCTL_TYPE_SCNDS)?
		    SCNDS_SCALE : BYTES_SCALE;

		if (retv->att_val_type != ATT_VAL_TYPE_LIST)
			goto out;

		/*
		 * More work need to be done to handle these cases
		 */

		for (i = 0; i < lst_numelements(retv->att_val_values); i++) {
			atvl = atv = lst_at(retv->att_val_values, i);

			/*
			 * Continue if not a list and the second value
			 * is not a scaler value
			 */
			if (atv->att_val_type != ATT_VAL_TYPE_LIST ||
			    lst_numelements(atv->att_val_values) < 2 ||
			    (atv = lst_at(atv->att_val_values, 1)) == NULL ||
			    atv->att_val_type != ATT_VAL_TYPE_VALUE) {
				continue;
			}
			values = attrib_val_tostring(atv);
			if (util_val2num(values, scale, errlst,
			    &num, &mod, &unit) == 0) {

				attrib_val_free(atv);
				atv = ATT_VAL_ALLOC_VALUE(num);
				lst_replace_at(atvl->att_val_values, 1, atv);
				free(mod);
				free(unit);

			} else {
				free(values);
				attrib_free(ret);
				free(ret);
				ret = NULL;
				goto out;
			}
			free(values);
		}
	}

out:
	free(mat);
	return (ret);
}
