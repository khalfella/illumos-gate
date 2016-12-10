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

#include <project.h>

#include <ctype.h>

#include "util.h"



#define BOSTR_REG_EXP	"^"
#define EOSTR_REG_EXP	"$"
#define FLTNM_REG_EXP	"([[:digit:]]+(\\.[[:digit:]]+)?)"
#define	MODIF_REG_EXP	"([kmgtpeKMGTPE])?"
#define UNIT__REG_EXP	"([bsBS])?"
#define TOKEN_REG_EXP	"[[:alnum:]_./=+-]*"
#define VALUE_REG_EXP	FLTNM_REG_EXP MODIF_REG_EXP UNIT__REG_EXP

#define TO_EXP(X)	BOSTR_REG_EXP X EOSTR_REG_EXP
#define TOKEN_EXP	TO_EXP(TOKEN_REG_EXP)
#define VALUE_EXP	TO_EXP(VALUE_REG_EXP)

#define BYTES_SCALE	1
#define SCNDS_SCALE	2

void *
util_safe_realloc(void *ptr, size_t sz)
{
	if ((ptr = realloc(ptr, sz)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "projadd: error reallocating %d bytes of memory"), sz);
		exit(1);
	}
	return (ptr);
}


void *
util_safe_malloc(size_t sz)
{
 	char *ptr;
	if ((ptr = malloc(sz)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "error allocating %d bytes of memory\n"), sz);
		exit(1);
        }
        return (ptr);
}

void *
util_safe_zmalloc(size_t sz)
{
	return (memset(util_safe_malloc(sz), 0, sz));
}

void
util_print_errmsgs(lst_t *errlst)
{
	char *errmsg;
	while(!lst_is_empty(errlst)) {
		errmsg = lst_at(errlst, 0);
		(void) lst_remove(errlst, errmsg);
		(void) fprintf(stderr, "%s\n", errmsg);
		free(errmsg);
	}
}


void
util_add_errmsg(lst_t *errlst, char *format, ...)
{
	va_list args;
	char *errmsg;

	va_start(args, format);
	if(vasprintf(&errmsg, format, args) < 0) {
		va_end(args);
		(void) fprintf(stderr, gettext(
		   "error allocating memory\n"));
		exit(1);
	}
	va_end(args);
	lst_insert_tail(errlst, errmsg);
}

char *
util_str_append(char *str, int nargs, ...)
{
	va_list ap;
	int i, len;
	char *s;

	if (str == NULL)
		str = util_safe_zmalloc(1);

	len = strlen(str) + 1;
	va_start(ap, nargs);
	for(i = 0; i < nargs; i++) {
		s = va_arg(ap, char*);
		len += strlen(s);
		str = util_safe_realloc(str, len);
		(void) strcat(str, s);
	}
	va_end(ap);
	return str;
}

char *
util_substr(regex_t *reg, regmatch_t *mat, char *str, int idx)
{
	int mat_len;
	char *ret;
	
	if (idx < 0 || idx > reg->re_nsub)
		return (NULL);

	mat_len = mat[idx].rm_eo - mat[idx].rm_so;

	ret = util_safe_malloc(mat_len + 1);
	*ret = '\0';

	(void) strlcpy(ret, str + mat[idx].rm_so, mat_len + 1);
	return (ret);
}

int
util_scale(char *unit, int scale, uint64_t *res, lst_t *errlst)
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
				util_add_errmsg(errlst, gettext(
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
				util_add_errmsg(errlst, gettext(
				    "Invalid unit: \"%s\""), unit);
				return (1);
		}
		return (0);
	}

	util_add_errmsg(errlst, gettext(
	    "Invalid scale: %d"), scale);

	return (1);	
}


int
util_val2num(char *value, int scale, lst_t *errlst, char **retnum,
    char **retmod, char **retunit)
{
	
	int ret = 1;
	regex_t valueexp;
	regmatch_t *mat;
	int nmatch;
	char *num, *modifier, *unit;
	char *ptr;

	uint64_t mul64;
	long double dnum;

	*retnum = *retmod = *retunit = NULL;

	if (regcomp(&valueexp, VALUE_EXP, REG_EXTENDED) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Failed to compile regex: '%s'"), VALUE_EXP);
		return (1);
	}

	nmatch = valueexp.re_nsub + 1;
	mat = util_safe_malloc(nmatch * sizeof(regmatch_t));

	if (regexec(&valueexp, value, nmatch, mat, 0) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Invalid value: '%s'"), value);
		regfree(&valueexp);
		return (1);
	}
	regfree(&valueexp);

	num = util_substr(&valueexp, mat, value, 1);
	modifier = util_substr(&valueexp, mat, value, 3);
	unit = util_substr(&valueexp, mat, value, 4);

	if ((num == NULL || modifier == NULL || unit == NULL) ||
	    (strlen(modifier) == 0 && strchr(num, '.') != NULL) ||
	    (util_scale(modifier, scale, &mul64, errlst) != 0) ||
	    (scale == BYTES_SCALE && *unit != '\0' && tolower(*unit) != 'b') ||
	    (scale == SCNDS_SCALE && *unit != '\0' && tolower(*unit) != 's')) {
		util_add_errmsg(errlst, gettext( "Error near: \"%s\""),
		    value);
		goto out;
	}

	dnum = strtold(num, &ptr);
	if (dnum == 0 &&
	    (errno == EINVAL || errno == ERANGE ) &&
	    *ptr != '\0' ) {
		util_add_errmsg(errlst, gettext("Invalid value:  \"%s\""),
		    value);
		goto out;
	}

	if (UINT64_MAX / mul64 <= dnum) {
		util_add_errmsg(errlst, gettext("Too big value:  \"%s\""),
		    value);
		goto out;
	}

	if (asprintf(retnum, "%llu",
	    (unsigned long long)(mul64 * dnum)) == -1) {
		goto out;
	}

	free(num);
	*retmod = modifier;
	*retunit = unit;
	return (0);
out:
	free(num); free(modifier); free(unit);
	return (ret);
}

void
util_free_tokens(char **tokens)
{
	char *token;
	while ((token = *tokens++) != NULL) {
		free(token);
	}
}

char **
util_tokenize(char *values, lst_t *errlst)
{
	char *token, *t;
	char *v;
	regex_t tokenexp;
	char **ctoken, **tokens = NULL;

	if (regcomp(&tokenexp, TOKEN_EXP, REG_EXTENDED | REG_NOSUB) != 0)
		return (tokens);

	/* assume each character will be a token + NULL terminating value*/
	ctoken = tokens = util_safe_malloc(
	    (strlen(values) + 1) * sizeof(char *));
	token = util_safe_malloc(strlen(values) + 1);

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
				util_add_errmsg(errlst, gettext(
				    "Invalid Character at or near \"%s\""),
				    token); 
				util_free_tokens(tokens);
				UTIL_FREE_SNULL(tokens);
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
