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

#include "projadd.h"

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

void *
safe_malloc(size_t sz)
{
 	char *ptr;
	if ((ptr = malloc(sz)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "projadd: error allocating %d bytes of memory"),
		    sz);
		exit(1);
        }
        return (ptr);
}

char *
projent_extract_matched_substr(regex_t *reg, regmatch_t *mat, char *str,
    int idx)
{
	int mat_len;
	char *ret;
	
	if (idx < 0 || idx > reg->re_nsub)
		return (NULL);

	mat_len = mat[idx].rm_eo - mat[idx].rm_so;

	ret = safe_malloc(mat_len + 1);
	*ret = '\0';

	strlcpy(ret, str + mat[idx].rm_so, mat_len + 1);
	return (ret);
}


char
*projent_value2string(attrib_val_t *val)
{
	char *ret = NULL;
	char *vstring;
	int i, len;
	attrib_val_t *v;
	switch(val->att_val_type) {
		case ATT_VAL_TYPE_NULL:
			return strdup("");
		break;
		case ATT_VAL_TYPE_VALUE:
			return strdup(val->att_val_value);
		break;
		case ATT_VAL_TYPE_LIST:
			ret = safe_malloc(3);
			strcpy(ret, "(");
			len = 3;
			for (i = 0, v = lst_at(val->att_val_values, 0);
			    v != NULL;
			    i++, v = lst_at(val->att_val_values, i)) {
				vstring = projent_value2string(v);
				len += strlen(vstring);
				ret = realloc(ret, len);
				strcat(ret, vstring);
				free(vstring);
			}
			strcat(ret, ")");
			return (ret);
		break;
	}


	/**** should not reach here *****/
	return (ret);
}

char
*projent_attribute2string(attrib_t *att)
{
	char *ret, *vstring;
	int len;

	len = strlen(att->att_name) + 2;

	ret = safe_malloc(len);
	strlcpy(ret, att->att_name, len);

	if ((vstring = projent_value2string(att->att_value)) != NULL) {
		len += strlen(vstring) + 1;
		if ((strlen(vstring) > 0) &&
		    (ret = realloc(ret, len)) != NULL) {
			strcat(strcat(ret, "="), vstring);
			return (ret);
		}

		free(vstring);
	}

	return (ret);

}

char
*projent_attributes2string(lst_t *attrs)
{
	int i;
	attrib_t *att;
	char *ret = NULL;
	char *str;
	int len = 1;

	/**** DEBUG ****/
	printf("projent_attributes2string:\n"
	     "attrs->numelements = %d\n",
	     attrs->numelements);

	for (i = 0, att = lst_at(attrs, 0); att != NULL;
	    i++, att = lst_at(attrs, i)) {



		/**** DEBUG *****/
		printf("\ti = %d\n", i);



		if ((str = projent_attribute2string(att)) != NULL) {
			len += strlen(str);

			if (ret == NULL) {
				ret = safe_malloc(len);
				*ret = '\0';
			} else {
				ret = realloc(ret, len);
			}

			strcat(ret, str);
			/**DEBUG****/
			printf("\tstr = %s\n", str);
			free(str);
		} else {
			free(ret);
			ret = NULL;
			return (ret);
		}
	}

	/*** DEBUG ***/
	printf("\tret = %s\n", ret);
	return (ret);
}

void
projent_free_attribute_value(attrib_val_t *att_val)
{
	attrib_val_t *val;

	if (att_val->att_val_type == ATT_VAL_TYPE_VALUE) {
		free(att_val->att_val_value);
	} else if (att_val->att_val_type == ATT_VAL_TYPE_LIST) {
		while (!lst_is_empty(att_val->att_val_values)) {
			val = lst_at(att_val->att_val_values, 0);
			lst_remove(att_val->att_val_values, val);
			projent_free_attribute_value(val);
			free(val);
		}
		free(att_val->att_val_values);
	}
}

void
projent_free_attribute(attrib_t *att)
{
	free(att->att_name);
	if (att->att_value != NULL) {
		projent_free_attribute_value(att->att_value);
		free(att->att_value);
	}
}
void
projent_free_attributes_lst(lst_t *attribs)
{
	attrib_t *att;
	while (!lst_is_empty(attribs)) {
		att = lst_at(attribs, 0);
		lst_remove(attribs, att);
		projent_free_attribute(att);
		free(att);
	}
}
int
rctl_get_info(char *name, rctl_info_t *pinfo)
{
	rctlblk_t *blk1, *blk2, *tmp;
	rctl_priv_t priv;

	blk1 = blk2 = tmp = NULL;
	blk1 = safe_malloc(rctlblk_size());
	blk2 = safe_malloc(rctlblk_size());

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
int
projent_scale(char *unit, int scale, uint64_t *res, list_t *errlst)
{
	if (scale == BYTES_SCALE) {

		switch(tolower(*unit)) {
			case 'k':
				*res = 1024ULL;
				break;
			case 'm':
				*res = 1048576ULL;
				break;
			case 'g':
				*res = 1073741824ULL;
				break;
			case 't':
				*res = 1099511627776ULL;
				break;
			case 'p':
				*res = 1125899906842624ULL;
				break;
			case 'e':
				*res = 1152921504606846976ULL;
				break;
			case '\0':
				*res = 1ULL;
				break;
			default:
				projent_add_errmsg(errlst, gettext(
				    "Invalid unit: \"%s\""), unit);
				return (1);
		}

		return (0);

	} else if (scale == SCNDS_SCALE) {

		switch(tolower(*unit)) {
			case 'k':
				*res = 1000ULL;
				break;
			case 'm':
				*res = 1000000ULL;
				break;
			case 'g':
				*res = 1000000000ULL;
				break;
			case 't':
				*res = 1000000000000ULL;
				break;
			case 'p':
				*res = 1000000000000000ULL;
				break;
			case 'e':
				*res = 1000000000000000000ULL;
				break;
			case '\0':
				*res = 1ULL;
				break;
			default:
				projent_add_errmsg(errlst, gettext(
				    "Invalid unit: \"%s\""), unit);
				return (1);
		}
		return (0);
	}

	
	projent_add_errmsg(errlst, gettext(
	    "Invalid scale: %d"), scale);

	return (1);	
}


int
projent_val2num(char *value, int scale, list_t *errlst, char **retnum, char **retmod,
    char **retunit)
{
	
	int ret = 1;
	regex_t valueexp;
	regmatch_t *mat;
	int nmatch;
	char *num, *modifier, *unit;
	char *ptr;

	uint64_t mul64, num64;
	long double dnum;

	*retnum = *retmod = *retunit = NULL;

	if (regcomp(&valueexp, VALUE_EXP, REG_EXTENDED) != 0) {
		projent_add_errmsg(errlst, gettext(
		    "Failed to compile regex: '%s'"), VALUE_EXP);
		goto out1;
	}

	nmatch = valueexp.re_nsub + 1;
	mat = safe_malloc(nmatch * sizeof(regmatch_t));

	if (regexec(&valueexp, value, nmatch, mat, 0) != 0) {
		projent_add_errmsg(errlst, gettext(
		    "Invalid value: '%s'"), value);
		goto out2;
	}


	num = projent_extract_matched_substr(&valueexp, mat, value, 1);
	modifier = projent_extract_matched_substr(&valueexp, mat, value, 3);
	unit = projent_extract_matched_substr(&valueexp, mat, value, 4);

	if ((num == NULL || modifier == NULL || unit == NULL) ||
	    (strlen(modifier) == 0 && strchr(num, '.') != NULL) ||
	    (projent_scale(modifier, scale, &mul64, errlst) != 0) ||
	    (scale == BYTES_SCALE && strlen(unit) > 0 && !SEQUAL(unit, "b")) ||
	    (scale == SCNDS_SCALE && strlen(unit) > 0 && !SEQUAL(unit, "s"))) {
		projent_add_errmsg(errlst, gettext( "Error near: \"%s\""),
		    value);
		free(num);
		free(modifier);
		free(unit);
		goto out2;
	}

	dnum = strtold(num, &ptr);
	if (errno == EINVAL || errno == ERANGE || *ptr != '\0' ) {
		projent_add_errmsg(errlst, gettext("Invalid value:  \"%s\""),
		    value);
		free(num);
		free(modifier);
		free(unit);
		goto out2;
	}



	if (UINT64_MAX / mul64 <= dnum) {
		projent_add_errmsg(errlst, gettext("Too big value:  \"%s\""),
		    value);
		free(num);
		free(modifier);
		free(unit);
		goto out2;
	}


	asprintf(retnum, "%llu", (unsigned long long)(mul64 * dnum));
	free(num);
	*retmod = modifier;
	*retunit = unit;
	ret = 0;

out2:
	regfree(&valueexp);
out1:
	return (ret);
}

void
projent_free_tokens_array_elements(char **tokens)
{
	char *token;
	while ((token = *tokens++) != NULL) {
		free(token);
	}
}

char **
projent_tokenize_attribute_values(char *values, list_t *errlst)
{
	char *token, *t;
	char *v;

	regex_t tokenexp;

	char **ctoken, **tokens = NULL;

	if (regcomp(&tokenexp, TOKEN_EXP, REG_EXTENDED | REG_NOSUB) != 0)
		return (tokens);

	/* assume each character will be a token + NULL terminating value*/
	ctoken = tokens = safe_malloc((strlen(values) * sizeof(char *)) + 1);
	token = safe_malloc(strlen(values) + 1);

	v = values;
	*ctoken = NULL;
	while (v < (values + strlen(values))) {

		/* get the next token */
		t = token;
		while ((*t = *v++) != '\0') {
			if (*t == '(' || *t == ')' || *t == ','|| 
			    *v == '(' || *v == ')' || *v == ',') {
				*++t = '\0';
				break;
			}
			t++;
		}

		if (strcmp(token, "(") != 0 && strcmp(token, ")") != 0 &&
		    strcmp(token, ",") != 0 ) {
			if (regexec(&tokenexp, token, 0, NULL, 0) != 0) {
				projent_add_errmsg(errlst, gettext(
				    "Invalid Character at or near \"%s\""),
				    token); 
				projent_free_tokens_array_elements(tokens);
				free(tokens);
				tokens = NULL;
				goto out1;
			}
		}
		*ctoken++ = strdup(token);
		*ctoken = NULL;
	}

out1:
	free(token);
	regfree(&tokenexp);
	return (tokens);
}

int
projent_attribute_value_to_list(attrib_val_t *atv)
{
	void *val;
	int type;
	attrib_val_t *mat;

	if (atv->att_val_type == ATT_VAL_TYPE_LIST)
		return (0);

	val = atv->att_val_value;
	type = atv->att_val_type;

	atv->att_val_type = ATT_VAL_TYPE_LIST;
	atv->att_val_values = safe_malloc(sizeof(lst_t));
	lst_create(atv->att_val_values);

	if (type == ATT_VAL_TYPE_VALUE && val != NULL) {
		mat = safe_malloc(sizeof(attrib_val_t));
		mat->att_val_type = type;
		mat->att_val_value = val;
		lst_insert_tail(atv->att_val_values, mat);
	}
	return (0);
}

int
projent_attribute_value_add_token(attrib_val_t *atv, char *token)
{
	attrib_val_t *nat;
	if (atv->att_val_type == ATT_VAL_TYPE_VALUE) {

		/* convert this to LIST attribute */
		projent_attribute_value_to_list(atv);
	}

	if (atv->att_val_type == ATT_VAL_TYPE_NULL) {
		/* convert this to VALUE attribute */
		atv->att_val_value = strdup(token);
		atv->att_val_type = ATT_VAL_TYPE_VALUE;
	} else if (atv->att_val_type == ATT_VAL_TYPE_LIST) {
		/* append token to the list */
		nat = safe_malloc(sizeof(attrib_val_t));
		nat->att_val_type = ATT_VAL_TYPE_VALUE;
		nat->att_val_value = strdup(token);
		lst_insert_tail(atv->att_val_values, nat);
	}

	return (0);
}

attrib_val_t
*projent_parse_attribute_values(char *values, list_t *errlst)
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


	char *prev = "";
	int parendepth = 0;

	if ((tokens =
	    projent_tokenize_attribute_values(values, errlst)) == NULL) {
		goto out1;
	}

	ret = safe_malloc(sizeof(attrib_val_t));
	ret->att_val_type = ATT_VAL_TYPE_NULL;
	ret->att_val_value = NULL;

	at = ret;

	lst_create(&stk);
	usedtokens = safe_malloc(1);
	*usedtokens = '\0';


	/**** DEBUG ****/
	printf("projent_parse_attribute_values:\n");


	/* walk the tokens list */
	for (i = 0; (token = tokens[i]) != NULL; i++) {

		usedtokens = realloc(usedtokens,
		    strlen(usedtokens) + strlen(token) + 1);
		strcat(usedtokens, token);

		/**** DEBUG ****/
		printf("\ntoken ===== %s\n", token);

		if (SEQUAL(token, ",")) {
			if (SEQUAL(prev, ",") || SEQUAL(prev, "(") ||
			    SEQUAL(prev, "")) {
				projent_attribute_value_add_token(at, "");
			}
			projent_attribute_value_to_list(at);
			prev = ",";
		} else if (SEQUAL(token, "(")) {
			if (!(SEQUAL(prev, "(") ||
			    SEQUAL(prev,",") ||
			    SEQUAL(prev, ""))) {
				projent_add_errmsg(errlst, gettext(
				    "\"%s\" <- \"(\" unexpected"), usedtokens); 
				/*
				 * properly free this ret LATER ON
				 */
				free(ret);
				ret = NULL;
				break;
			}

			switch(at->att_val_type) {
				case ATT_VAL_TYPE_VALUE:
					/* Something Wrong */
					printf("Something WRONGGGGG\n");
					exit(13);
				break;
				case ATT_VAL_TYPE_NULL:
					/* Make is a LIST attrib */
					projent_attribute_value_to_list(at);
					break;
				case ATT_VAL_TYPE_LIST:
					/* Allocate NULL node */
					nat = safe_malloc(
					    sizeof(attrib_val_t));
					nat->att_val_type = ATT_VAL_TYPE_NULL;
					nat->att_val_value = NULL;
					lst_insert_tail(
					    at->att_val_values, nat);
					/* push at down one level */
					lst_insert_head(&stk, at);
					at = nat;
				break;
			}
			parendepth++;
			prev = "(";
		} else if (SEQUAL(token, ")")) {
			if (parendepth <= 0) {
				projent_add_errmsg(errlst, gettext(
				    "\"%s\" <- \")\" unexpected"), usedtokens); 
				/* deal with this later on */
				free(ret);
				break;
			}
			if (SEQUAL(prev, ",") || SEQUAL(prev, "(")) {
				projent_attribute_value_add_token(at, "");
			}
			oat = at;
			at = lst_at(&stk, 0);
			lst_remove(&stk, oat);
			parendepth--;
			prev = ")";
		} else {
			if (!(SEQUAL(prev, ",") ||
			    SEQUAL(prev, "(") ||
			    SEQUAL(prev, ""))) {
				projent_add_errmsg(errlst, gettext(
				    "\"%s\" <- \"%s\" unexpected"),
				    usedtokens, token); 
				/* deal with this free later on */
				free(ret);
				ret = NULL;
				break;
			}

			projent_attribute_value_add_token(at, token);
			prev = token;
		}
	}

	if (parendepth != 0) {
		projent_add_errmsg(errlst, gettext(
		    "\"%s\" <- \")\" missing"),
		    ret, token); 
		free(ret);
	}

	if (SEQUAL(prev, ",") || SEQUAL(prev, "")) {
		switch(at->att_val_type) {
			case ATT_VAL_TYPE_NULL:
				/* Wrong thing */
				printf("Something WWERWERONG\n");
				exit(15);
			break;
			case ATT_VAL_TYPE_VALUE:
				/* convert this into LIST attr */
				mat = safe_malloc(sizeof(attrib_val_t));
				mat->att_val_type = ATT_VAL_TYPE_VALUE;
				mat->att_val_value = at->att_val_value;
				mat->att_val_values = NULL;
				at->att_val_type = ATT_VAL_TYPE_LIST;
				at->att_val_value = NULL;
				at->att_val_values =
				    safe_malloc(sizeof(lst_t));
				lst_create(at->att_val_values);
				lst_insert_tail(at->att_val_values, mat);
			/* fallthrough */
			case ATT_VAL_TYPE_LIST:
				nat = safe_malloc(sizeof(attrib_val_t));
				nat->att_val_type = ATT_VAL_TYPE_VALUE;
				nat->att_val_value = strdup("");
				nat->att_val_values = NULL;
				lst_insert_tail(at->att_val_values, nat);
			break;
		}
	}

	projent_free_tokens_array_elements(tokens);
	free(tokens);
	free(usedtokens);
out1:
	return (ret);
}

attrib_t
*projent_parse_attribute(regex_t *attrbexp, regex_t *atvalexp, char *att,
    list_t *errlst)
{
	int nmatch = MAX_OF(attrbexp->re_nsub, atvalexp->re_nsub) + 1;
	attrib_t *ret = NULL;
	char *values = NULL;
	int vstart, vend, vlen;
	int nstart, nend, nlen;
	int retlen;
	int vidx, nidx;

	regmatch_t *mat = safe_malloc(nmatch * sizeof(regmatch_t));
	ret = safe_malloc(sizeof(attrib_t));
	ret->att_name = NULL;
	ret->att_value = NULL;


	/** DEBUG **/
	printf("projent_parse_attribute:\n"
	    "\tatt = %s\n",
	    att);



	if (regexec(attrbexp, att, attrbexp->re_nsub + 1 , mat, 0) == 0) {
		ret->att_name = strdup(att);
		ret->att_value = safe_malloc(sizeof(attrib_val_t));
		ret->att_value->att_val_type = ATT_VAL_TYPE_NULL;
		ret->att_value->att_val_value = NULL;
		ret->att_value->att_val_values = NULL;
	} else if (regexec(atvalexp, att, atvalexp->re_nsub + 1, mat, 0) == 0) {
		vidx = atvalexp->re_nsub;
		vlen = mat[vidx].rm_eo - mat[vidx].rm_so;
		nidx = atvalexp->re_nsub - 3;

		if (vlen > 0) {
			ret->att_name = projent_extract_matched_substr(
			    atvalexp, mat, att, nidx); 
			values = projent_extract_matched_substr(atvalexp, mat,
			    att, vidx);
			ret->att_value = projent_parse_attribute_values(
			    values, errlst);
			free(values);
		} else {
			/* the value is empty, just return att name */
			ret->att_name = projent_extract_matched_substr(
			    atvalexp, mat, att, nidx); 
			ret->att_value = safe_malloc(sizeof(attrib_val_t));
			ret->att_value->att_val_type = ATT_VAL_TYPE_NULL;
			ret->att_value->att_val_value = NULL;
			ret->att_value->att_val_values = NULL;
		}
	} else {
		projent_add_errmsg(errlst, gettext(
		    "Invalid attribute \"%s\""), att);
	}


	/*
	 * 1- Handle the case of rcap.max-rss
	 * 2- Handle resource controls caeses
	 */


	/**** DEBUG ***/
/*	printf("\t ret = %x\n"
	    "\tret->att_name = %s\n"
	    "\tret->att_value = %s\n"
	    "\tret->att_value->att_val_type = %d\n"
	    "\tret->att_value->att_val_value = %s\n"
	    "\tret->att_value->att_val_values = %s\n",
	    ret, ret->att_name, ret->att_value,
	    ret->att_value->att_val_type,
	    ret->att_value->att_val_value,
	    ret->att_value->att_val_values);
*/
	
	free(mat);
	return (ret);
}

lst_t
*projent_parse_attributes(char *attribs, list_t *errlst)
{
	char *sattrs, *attrs, *att;
	regex_t attrbexp, atvalexp;

	attrib_t *natt = NULL;
	lst_t *ret = NULL;

	ret = safe_malloc(sizeof(lst_t));
	(void) lst_create(ret);
	

	if (regcomp(&attrbexp, ATTRB_EXP, REG_EXTENDED) != 0)
		goto out1;
	if (regcomp(&atvalexp, ATVAL_EXP, REG_EXTENDED) != 0)
		goto out2;


	/**** DEBUG **/
	printf("projent_parse_attributes:\n"
	    "\tattrib = %s\n",
	    attribs);

	sattrs = attrs = strdup(attribs);

	while ((att = strsep(&attrs, ";")) != NULL) {
		if (*att == '\0')
			continue;
		if ((natt = projent_parse_attribute(&attrbexp,
		    &atvalexp, att, errlst)) == NULL) {
			projent_free_attributes_lst(ret);
			free(ret);
			ret = NULL;
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


int
projent_validate_usrgrp(char *usrgrp, char *name, list_t *errlst)
{
	char *sname = name;
	if (strcmp(name, "*") == 0 || strcmp(name, "!*") == 0)
		return (0);
	if (*name == '!' && name[1] != '\0')
		name++;
	while (isalnum(*name))
		name++;

	if (*name != '\0') {
		projent_add_errmsg(errlst, gettext(
		    "Invalid %s \"%s\": should not contain '%c'"),
		    usrgrp, sname, *name);
		return (1);
	}
	return (0);
}

char *
projent_parse_usrgrp(char *usrgrp, char *nlist, list_t *errlst)
{
	char *ulist, *susrs, *usrs, *usr;

	susrs = usrs = strdup(nlist);
	ulist = safe_malloc(strlen(nlist) + 1);
	*ulist = '\0';

	while ((usr = strsep(&usrs, " ,")) != NULL) {
		if (*usr == '\0')
			continue;
		/* Error validating this user/group */
		if (projent_validate_usrgrp(usrgrp, usr, errlst) != 0) {
			free(ulist);
			ulist = NULL;
			break;
		}
		/*
		 * If it is not the first user in the list,
		 * append ',' before appending the username. 
		 */
		ulist = (*ulist != '\0') ? strcat(ulist, ",") : ulist;

		ulist = strcat(ulist, usr);
	}

	free(susrs);
	return (ulist);
}


int
projent_parse_comment(char *comment, list_t *errlst)
{
	char *cmt = comment;
	while (*cmt) {
		if (*cmt++ == ':') {
			projent_add_errmsg(errlst, gettext(
			    "Invalid Comment \"%s\": should not contain ':'"),
			    comment);
		}
	
	}
	return (0);
}

int
projent_validate_unique_id(list_t *plist, projid_t projid,list_t *errlst)
{
	projent_t *ent;
	for (ent = list_head(plist); ent != NULL;
	    ent = list_next(plist, ent)) {
		if (ent->projid == projid) {
			projent_add_errmsg(errlst, gettext(
			    "Duplicate projid \"%d\""), projid);
			return (1);
		}
	}
	return (0);
}
int
projent_validate_projid(projid_t projid, list_t *errlst)
{
	if (projid < 0) {
		projent_add_errmsg(errlst, gettext(
		    "Invalid projid \"%d\": "
		    "must be >= 100"),
		    projid);
		return (1);
	}
	return (0);
}

int
projent_parse_projid(char *projidstr, projid_t *pprojid, list_t *errlst)
{
	char *ptr;

	*pprojid = strtol(projidstr, &ptr, 10);

	/* projid should be a positive number */
	if (errno == EINVAL || errno == ERANGE || *ptr != '\0' ) {
		projent_add_errmsg(errlst, gettext("Invalid project id:  %s"),
		    projidstr);
		return (1);
	}

	/* projid should be less than UID_MAX */
	if (*pprojid > INT_MAX) {
		projent_add_errmsg(errlst, gettext(
		    "Invalid projid \"%s\": must be <= %d"), projidstr, INT_MAX);
		return (1);
	}
	return (0);
}

int
projent_validate_unique_name(list_t *plist, char *pname, list_t *errlst)
{
	projent_t *ent;
	for (ent = list_head(plist); ent != NULL;
	    ent = list_next(plist, ent)) {
		if (strcmp(ent->projname, pname) == 0) {
			projent_add_errmsg(errlst, gettext(
			    "Duplicate project name \"%s\""), pname);
			return (1);
		}
	}
	return (0);
		
}

int
projent_parse_name(char *pname, list_t *errlst)
{
	char *name = pname;
	if (isupper(*pname) || islower(*pname)) {
		while (isupper(*pname) || islower(*pname) ||
		    isdigit(*pname) || *pname == '.' || *pname == '-') {
			pname++;
		}
	} else if (*pname == '\0') {
		/* Empty project names are not allowed */
		projent_add_errmsg(errlst, gettext(
		    "Invalid project name \"%s\", "
		    "empty project name"), name);
		return (1);
	}

	/* An invalid character was detected */
	if (*pname != '\0') {
		projent_add_errmsg(errlst, gettext(
		    "Invalid project name \"%s\", "
		    "contains invalid characters"), name);
		return (1);
	}

	if (strlen(name) > PROJNAME_MAX) {
		projent_add_errmsg(errlst, gettext(
		    "Invalid project name \"%s\", "
		    "name too long"), name);
		return (1);
	}

	return (0);
}

void projent_add_errmsg(list_t *errmsgs, char *format, ...)
{
	va_list args;
	char *errstr;
	errmsg_t *msg;

	if (errmsgs == NULL || format == NULL)
		return;

	va_start(args, format);
	vasprintf(&errstr, format, args);
	va_end(args);

	msg = safe_malloc(sizeof(errmsg_t));
	msg->msg = errstr;
	list_link_init(&msg->next);
	list_insert_tail(errmsgs, msg);
	va_end(args);
} 

void projent_print_errmsgs(list_t *errmsgs)
{
	errmsg_t *msg;
	while ((msg = list_head(errmsgs)) != NULL) {
		fprintf(stderr, "%s\n", msg->msg);
		list_remove(errmsgs, msg);
		free(msg->msg);
		free(msg);
	}
}

void
projent_print_ent(projent_t *ent)
{
	if (ent->projname) printf("projname = %s\n", ent->projname);
	printf("projid = %d\n", ent->projid);
	if (ent->comment) printf("comment = %s\n", ent->comment);
	if (ent->userlist) printf("userlist = %s\n", ent->userlist);
	if (ent->grouplist) printf("grouplist = %s\n", ent->grouplist);
	if (ent->attr) printf("attr = %s\n", ent->attr);
}

projent_t
*projent_parse(char *str) {
	char *s1;

	if (str == NULL)
		return (NULL);

	projent_t *ent = safe_malloc(sizeof(projent_t));
	list_link_init(&ent->next);

	if ((ent->projname = strsep(&str, ":")) != NULL &&
	    (s1 = strsep(&str, ":")) != NULL &&
	    ((ent->projid = atoi(s1)) != 0 || errno != EINVAL) &&
	    (ent->comment = strsep(&str, ":")) != NULL &&
	    (ent->userlist = strsep(&str, ":")) != NULL &&
	    (ent->grouplist = strsep(&str, ":")) != NULL &&
	    (ent->attr = strsep(&str, ":")) != NULL &&
	    strsep(&str, ":") == NULL) {
		return (ent);
	}

	free(ent);
	return (NULL);
}


void
projent_free_list(list_t *plist) {
	projent_t *ent;
	while ((ent = list_head(plist)) != NULL) {
		list_remove(plist, ent);
		free(ent->projname);
		free(ent);
	}
}



list_t
*projent_get_list(char *projfile, list_t *perrlst)
{
	FILE *fp;
	list_t *ret;
	int read, line = 0;
	char *buf = NULL, *ptr, *nlp;
	size_t cap = 0;
	projent_t *ent;

	ret = safe_malloc(sizeof(list_t));
	list_create(ret, sizeof(projent_t), offsetof(projent_t, next));

	if ((fp = fopen(projfile, "r")) == NULL) {
		if (errno == ENOENT) {
			/*
			 * There is no project file,
			 * return an empty list
			 */
			return (ret);
		} else {
			/* Report the error unable to open the file */
			projent_add_errmsg(perrlst,
			    gettext("Cannot open %s: %s"),
			    projfile, strerror(errno));

			/* destory and free the to-be-returned list */
			list_destroy(ret);
			free(ret);
			return (NULL);
		}
	}

	while((getline(&buf, &cap, fp)) != -1 && ++line) {

		ptr = strdup(buf);	
		if ((nlp = strchr(ptr, '\n')) != NULL)
			*nlp = '\0';

		if ((ent = projent_parse(ptr)) != NULL) {
			list_insert_tail(ret, ent);
		} else {
			/* Report the error */
			projent_add_errmsg(perrlst,
			    gettext("Error parsing: %s line: %d: \"%s\""),
			    projfile, line, ptr);

			/* free the allocated resources */
			projent_free_list(ret);
			list_destroy(ret);
			free(ret);
			free(ptr);
			ret = NULL;
			break;
		}
	}

	free(buf);
	fclose(fp);
	return (ret);
}
