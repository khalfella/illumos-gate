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

#define FLTNM_REG_EXP	"([[:digit:]](\.[[:digit:]]+)?)"
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
projent_val2num(char *value, int scale, char **retnum, char **retmod,
    char **retunit)
{
	
	char *ret = NULL;
	regex_t valueexp;
	regmatch_t *mat;
	int nmatch;
	char *num, *modifier, *type;
	int s_num, e_num, len_num;
	int s_mod, e_mod, len_mod;
	int s_typ, e_typ, len_typ;

	*retnum = *retmod = *retunit = NULL;

	/*

	if (regcomp(&valueexp, VALUE_EXP, REG_EXTENDED) != 0)
		goto out1;

	nmatch = valueexp.re_nsub + 1;
	mat = safe_malloc(nmatch * sizeof(regmatch_t));

	if (regexec(&valueexp, value, nmatch, mat, 0) == 0) {
	}



	regfree(&valueexp);
out1:
	return (ret);
	*/

	return (0);
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

char *
projent_parse_attribute_values_tokens(char *values, list_t *errlst)
{
	char **tokens = NULL;
	char *token;
	int i;

	if ((tokens =
	    projent_tokenize_attribute_values(values, errlst)) == NULL) {
		goto out1;
	}


	/* walk the tokens list */
	for (i = 0; (token = tokens[i]) != NULL; i++) {
		printf("***** token[%d] = \"%s\"\n", i, token);
	}


	projent_free_tokens_array_elements(tokens);
	free(tokens);
out1:
	/* fake return */
	return strdup(values);
}
char *
projent_parse_attribute_values(char *name, char *values, list_t *errlst)
{
	/*
	 * 1 - Make sure that the value is in the acceptable grammatical shape.
	 * This should call the parser to parse the value and put make sure
	 * it is ok. else return NULL and report the error in errlst.
	 */
	char *pvalues;
	if ((pvalues =
	    projent_parse_attribute_values_tokens(values, errlst)) != NULL) {
		free(pvalues);
	}

	/*
	 * 2- Process the part of "rcap.max-rss". Double check and adjust the
	 * value if needed
	 */

	/*
	 * 3- Process the part of RctlRules. Make sure it is ok.
	 */

	/*
	 * 4- Finally, check if there is any scaling required
	 */


	/*
	 * 5- Given all the above, return the value in the correct shape
	 */
	
	return strdup(values);
}

char *
projent_parse_attribute(regex_t *attrbexp, regex_t *atvalexp, char *att,
    list_t *errlst)
{
	int nmatch = MAX_OF(attrbexp->re_nsub, atvalexp->re_nsub) + 1;
	char *ret = NULL;
	char *nvalues, *name, *values = NULL;
	int vstart, vend, vlen;
	int nstart, nend, nlen;
	int retlen;
	regmatch_t *mat = safe_malloc(nmatch * sizeof(regmatch_t));


	if (regexec(attrbexp, att, attrbexp->re_nsub + 1 , mat, 0) == 0) {
		ret = strdup(att);
	} else if (regexec(atvalexp, att, atvalexp->re_nsub + 1, mat, 0) == 0) {
		vstart = mat[atvalexp->re_nsub].rm_so;
		vend = mat[atvalexp->re_nsub].rm_eo;
		vlen = vend - vstart;
		nstart = mat[atvalexp->re_nsub - 3].rm_so;
		nend = mat[atvalexp->re_nsub - 3].rm_eo;
		nlen = nend - nstart;
		if (vlen > 0) {

			values = safe_malloc(vlen + 1);
			strlcpy(values, (char *)(att + vstart), vlen + 1);
			name = safe_malloc(nlen + 1);
			strlcpy(name, (char *)(att + nstart), nlen + 1);

			if ((nvalues = projent_parse_attribute_values(
			    name, values, errlst)) != NULL) {
				retlen = nlen + 1 + strlen(nvalues) + 1;
				ret = safe_malloc(retlen);
				*ret = '\0';
				strcat(strcat(strcat(ret, name), "="), nvalues);
				free(nvalues);
			}
			free(values);
			free(name);
		} else {
			/* the value is empty, just return att name */
			ret = strdup(att);
			ret[nend] = '\0';
		}
	} else {
		projent_add_errmsg(errlst, gettext(
		    "Invalid attribute \"%s\""), att);
	}
	
	free(mat);
	return (ret);
}

char *
projent_parse_attributes(char *attribs, list_t *errlst)
{
	char *sattrs, *attrs, *att, *natt = NULL;
	regex_t attrbexp, atvalexp;
	char *ret = NULL;

	if (regcomp(&attrbexp, ATTRB_EXP, REG_EXTENDED) != 0)
		goto out1;
	if (regcomp(&atvalexp, ATVAL_EXP, REG_EXTENDED) != 0)
		goto out2;

	sattrs = attrs = strdup(attribs);
	ret = safe_malloc(strlen(attribs) + 1);
	*ret = '\0';


	while ((att = strsep(&attrs, ";")) != NULL) {
		if (*att == '\0')
			continue;
		if ((natt = projent_parse_attribute(&attrbexp,
		    &atvalexp, att, errlst)) == NULL) {
			free(ret);
			ret = NULL;
			break;
		}

		ret = (*ret != '\0') ? strcat(ret, ";") : ret;
		ret = strcat(ret, natt);
		free(natt);
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
