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


#include <ctype.h>

#include "projadd.h"

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
