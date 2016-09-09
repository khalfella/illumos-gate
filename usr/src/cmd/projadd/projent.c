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
#include "attrib.h"

#include "util.h"


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
#define PROJN_EXP	TO_EXP(IDENT_REG_EXP)


#define MAX_OF(X,Y)	(((X) > (Y)) ? (X) : (Y))


#define SEQUAL(X,Y)	(strcmp((X), (Y)) == 0)


#define BYTES_SCALE	1
#define SCNDS_SCALE	2

void
projent_sort_attributes(lst_t *attribs)
{
	attrib_sort_lst(attribs);
}

lst_t
*projent_parse_attributes(char *attribs, list_t *errlst)
{
	char *sattrs, *attrs, *att;
	regex_t attrbexp, atvalexp;

	attrib_t *natt = NULL;
	lst_t *ret = NULL;

	ret = util_safe_malloc(sizeof(lst_t));
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
		if ((natt = attrib_parse(&attrbexp,
		    &atvalexp, att, errlst)) == NULL) {
			attrib_free_lst(ret);
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
		util_add_errmsg(errlst, gettext(
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
	ulist = util_safe_malloc(strlen(nlist) + 1);
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
	int ret = 0;
	if (strchr(comment, ':') != NULL) {
		util_add_errmsg(errlst, gettext(
		    "Invalid Comment \"%s\": should not contain ':'"),
		    comment);
		ret = 1;
	}
	return (ret);
}

int
projent_validate_unique_id(list_t *plist, projid_t projid,list_t *errlst)
{
	projent_t *ent;
	for (ent = list_head(plist); ent != NULL;
	    ent = list_next(plist, ent)) {
		if (ent->projid == projid) {
			util_add_errmsg(errlst, gettext(
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
		util_add_errmsg(errlst, gettext(
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
		util_add_errmsg(errlst, gettext("Invalid project id:  %s"),
		    projidstr);
		return (1);
	}

	/* projid should be less than UID_MAX */
	if (*pprojid > INT_MAX) {
		util_add_errmsg(errlst, gettext(
		    "Invalid projid \"%s\": must be <= %d"),
		    projidstr, INT_MAX);
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
			util_add_errmsg(errlst, gettext(
			    "Duplicate project name \"%s\""), pname);
			return (1);
		}
	}
	return (0);
}

int
projent_parse_name(char *pname, list_t *errlst)
{
	int ret = 1;
	regex_t projnexp;
	if (regcomp(&projnexp, PROJN_EXP, REG_EXTENDED) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Failed to compile regular expression: \"%s\""),
		    PROJN_EXP);
		goto out;
	}

	if (regexec(&projnexp, pname, 0, NULL, 0) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Invalid project name \"%s\", "
		    "contains invalid characters"), pname);
	} else if (strlen(pname) > PROJNAME_MAX) {
		util_add_errmsg(errlst, gettext(
		    "Invalid project name \"%s\", "
		    "name too long"), pname);
	} else {
		ret = 0;
	}

	regfree(&projnexp);
out:
	return (ret);
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

void
projent_free(projent_t *ent)
{
	free(ent->projname);
	free(ent->comment);
	free(ent->userlist);
	free(ent->grouplist);
	free(ent->attr);
}

projent_t
*projent_parse(char *projstr) {
	char *s1, *str;

	if (projstr == NULL)
		return (NULL);

	str = strdup(projstr);

	projent_t *ent = util_safe_zmalloc(sizeof(projent_t));
	list_link_init(&ent->next);

	if ((ent->projname = strdup(strsep(&str, ":"))) != NULL &&
	    (s1 = strsep(&str, ":")) != NULL &&
	    ((ent->projid = atoi(s1)) != 0 || errno != EINVAL) &&
	    (ent->comment = strdup(strsep(&str, ":"))) != NULL &&
	    (ent->userlist = strdup(strsep(&str, ":"))) != NULL &&
	    (ent->grouplist = strdup(strsep(&str, ":"))) != NULL &&
	    (ent->attr = strdup(strsep(&str, ":"))) != NULL &&
	    strsep(&str, ":") == NULL) {
		goto done;
	}

	free(str);
	projent_free(ent);
	free(ent);
	ent = NULL;
done:
	return (ent);
}


void
projent_free_list(list_t *plist) {
	projent_t *ent;
	while ((ent = list_head(plist)) != NULL) {
		list_remove(plist, ent);
		projent_free(ent);
		free(ent);
	}
}



list_t
*projent_get_list(char *projfile, list_t *perrlst)
{
	FILE *fp;
	list_t *ret;
	int read, line = 0;
	char *buf = NULL, *nlp;
	size_t cap = 0;
	projent_t *ent;

	ret = util_safe_malloc(sizeof(list_t));
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
			util_add_errmsg(perrlst,
			    gettext("Cannot open %s: %s"),
			    projfile, strerror(errno));

			/* destory and free the to-be-returned list */
			list_destroy(ret);
			free(ret);
			return (NULL);
		}
	}

	while((getline(&buf, &cap, fp)) != -1 && ++line) {

		if ((nlp = strchr(buf, '\n')) != NULL)
			*nlp = '\0';

		if ((ent = projent_parse(buf)) != NULL) {
			list_insert_tail(ret, ent);
		} else {
			/* Report the error */
			util_add_errmsg(perrlst,
			    gettext("Error parsing: %s line: %d: \"%s\""),
			    projfile, line, buf);

			/* free the allocated resources */
			projent_free_list(ret);
			list_destroy(ret);
			free(ret);
			ret = NULL;
			break;
		}
	}

	free(buf);
	fclose(fp);
	return (ret);
}



char
*projent_attrib_lst_tostring(lst_t *lst)
{
	return (attrib_lst_tostring(lst));
}
