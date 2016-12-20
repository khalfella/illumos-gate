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
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <rctl.h>
#include <regex.h>
#include <ctype.h>

#include "projent.h"
#include "attrib.h"
#include "util.h"


#define	BOSTR_REG_EXP	"^"
#define	EOSTR_REG_EXP	"$"
#define	IDENT_REG_EXP	"[[:alpha:]][[:alnum:]_.-]*"
#define	PRJID_REG_EXP	"[[:digit:]]+"
#define	USERN_REG_EXP	"!?[[:alpha:]][[:alnum:]_.-]*"
#define	GRUPN_REG_EXP	"!?[[:alnum:]][[:alnum:]]*"

#define	TO_EXP(X)	BOSTR_REG_EXP X EOSTR_REG_EXP

#define	PROJN_EXP	TO_EXP(IDENT_REG_EXP)
#define	PRJID_EXP	TO_EXP(PRJID_REG_EXP)
#define	USERN_EXP	TO_EXP(USERN_REG_EXP)
#define	GRUPN_EXP	TO_EXP(GRUPN_REG_EXP)

/*ARGSUSED*/
int
projent_validate_name(char *pname, list_t *errlst)
{
	/* Do nothing, as any parse-able project name is valid */
	return (0);
}

/*ARGSUSED*/
int
projent_validate_comment(char *comment, list_t *errlst)
{
	/* Do nothing, as any parse-able project name is valid */
	return (0);
}

int
projent_validate_users(char *users, list_t *errlst)
{
	char *susrs, *usrs, *usr;
	char *u, **ulast, **ulist;
	int ret = 0;
	int i;

	susrs = usrs = util_safe_strdup(users);
	ulast = ulist = util_safe_zmalloc(
	    (strlen(users) + 1) * sizeof (char *));
	while ((usr = strsep(&usrs, ",")) != NULL) {
		if (*usr == '!')
			usr++;
		if ((*usr == '\0') || (strcmp(usr, "*") == 0))
			continue;

		if (getpwnam(usr) == NULL) {
			util_add_errmsg(errlst, gettext(
			    "User \"%s\" does not exist"), usr);
			ret = 1;
		}
		for (i = 0; (u = ulist[i]) != NULL; i++) {
			if (strcmp(u, usr) == 0) {
				util_add_errmsg(errlst, gettext(
				    "Duplicate user names \"%s\""), usr);
				ret = 1;
			}
		}
		/* Add the user to the temporary list if not found */
		if (u == NULL) {
			*ulast++ = usr;
		}
	}
	free(ulist);
	free(susrs);
	return (ret);
}

int
projent_validate_groups(char *groups, list_t *errlst)
{
	char *sgrps, *grps, *grp;
	char *g, **glast, **glist;
	int ret = 0;
	int i;

	sgrps = grps = util_safe_strdup(groups);
	glast = glist = util_safe_zmalloc(
	    (strlen(groups) + 1) * sizeof (char *));
	while ((grp = strsep(&grps, ",")) != NULL) {
		if (*grp == '!')
			grp++;
		if ((*grp == '\0') || (strcmp(grp, "*") == 0))
			continue;

		if (getgrnam(grp) == NULL) {
			util_add_errmsg(errlst, gettext(
			    "Group \"%s\" does not exist"), grp);
			ret = 1;
		}
		for (i = 0; (g = glist[i]) != NULL; i++) {
			if (strcmp(g, grp) == 0) {
				util_add_errmsg(errlst, gettext(
				    "Duplicate group names \"%s\""), grp);
				ret = 1;
			}
		}
		/* Add the group to the temporary list if not found */
		if (g == NULL) {
			*glast++ = grp;
		}
	}
	free(glist);
	free(sgrps);
	return (ret);
}

int
projent_validate_attributes(lst_t *attrs, list_t *errlst)
{
	return (attrib_validate_lst(attrs, errlst));
}

char *
projent_tostring(projent_t *ent)
{
	char *attrs;
	char *ret = NULL;
	if ((attrs = attrib_lst_tostring(ent->attrs)) != NULL) {
		(void) asprintf(&ret, "%s:%ld:%s:%s:%s:%s",
		    ent->projname,
		    ent->projid,
		    ent->comment,
		    ent->userlist,
		    ent->grouplist,
		    attrs);
		free(attrs);
	}
	return (ret);
}

int
projent_validate(projent_t *pent, int flags, list_t *errlst)
{
	char *str;

	(void) projent_validate_name(pent->projname, errlst);
	(void) projent_validate_projid(pent->projid, flags, errlst);
	(void) projent_validate_comment(pent->comment, errlst);
	(void) projent_validate_users(pent->userlist, errlst);
	(void) projent_validate_groups(pent->grouplist, errlst);
	(void) projent_validate_attributes(pent->attrs, errlst);

	if ((str = projent_tostring(pent)) == NULL) {
		util_add_errmsg(errlst, gettext("error allocating memory"));
	} else {
		if (strlen(str) > (PROJECT_BUFSZ - 2)) {
			util_add_errmsg(errlst, gettext(
			    "projent line too long"));
		}
		free(str);
	}
	return (list_is_empty(errlst) == 0);
}

int
projent_validate_list(list_t *plist, int flags, list_t *errlst)
{
	int i, idx;
	projent_t *ent;
	char *pnames = NULL;
	projid_t *pids = NULL;
	int ret = 0;

	idx = 0;
	for (ent = list_head(plist); ent != NULL; ent = list_next(plist, ent)) {
		/* Check for duplicate projname */
		if (pnames != NULL && strstr(pnames, ent->projname) != NULL) {
			util_add_errmsg(errlst, gettext(
			    "Duplicate project name %s"), ent->projname);
			ret++;
		}

		/* Check for duplicate projid if DUP is not allowed */
		if (!(flags & F_PAR_DUP) && pids != NULL) {
			for (i = 0; i < idx; i++) {
				if (ent->projid == pids[i]) {
					util_add_errmsg(errlst, gettext(
					    "Duplicate proid %d"), ent->projid);
					ret++;
					break;
				}
			}
		}

		/* Add the projname an projid to out temp list */
		pnames = UTIL_STR_APPEND2(pnames, "|", ent->projname);
		pids = util_safe_realloc(pids, (idx + 1) * sizeof (projid_t));
		pids[idx] = ent->projid;
		idx++;

		/* Validate the projet */
		ret += projent_validate(ent, flags, errlst);
	}

	free(pnames);
	free(pids);

	return (ret);
}

void
projent_free_attributes(lst_t *attribs)
{
	attrib_free_lst(attribs);
}

void
projent_sort_attributes(lst_t *attribs)
{
	attrib_sort_lst(attribs);
}

char *
projent_attrib_tostring(void *attrib)
{
	return (attrib_tostring(attrib));
}

char *
projent_attrib_lst_tostring(lst_t *lst)
{
	return (attrib_lst_tostring(lst));
}

void
projent_merge_attributes(lst_t **eattrs, lst_t *nattrs, int flags,
    list_t *errlst)
{
	attrib_merge_attrib_lst(eattrs, nattrs, flags, errlst);
}

lst_t *
projent_parse_attributes(char *attribs, int flags, list_t *errlst)
{
	return (attrib_parse_attributes(attribs, flags, errlst));
}

void
projent_merge_usrgrp(char *usrgrp, char **elist, char *nlist,
    int flags, list_t *errlst)
{
	char *res = NULL;
	char *seusrs, *eusrs, *eusr;
	char *snusrs, *nusrs, *nusr;
	char *sn1usrs, *n1usrs, *n1usr;
	char *sep;
	int i, j;

	sep = (flags & F_PAR_SPC) ? " ," : ",";

	if (flags & F_MOD_ADD) {
		res = util_safe_strdup(*elist);

		snusrs = nusrs = util_safe_strdup(nlist);
		while ((nusr = strsep(&nusrs, sep)) != NULL) {
			if (*nusr == '\0')
				continue;
			seusrs = eusrs = util_safe_strdup(*elist);
			while ((eusr = strsep(&eusrs, sep)) != NULL) {
				if (*eusr == '\0')
					continue;
				if (strcmp(eusr, nusr) == 0) {
					util_add_errmsg(errlst, gettext(
					    "Project already contains"
					    " %s \"%s\""), usrgrp, nusr);
					UTIL_FREE_SNULL(res);
					free(seusrs);
					free(snusrs);
					goto out;
				}
			}
			free(seusrs);
			/* Append nusr to the result */
			if (*res != '\0')
				res = UTIL_STR_APPEND1(res, ",");
			res = UTIL_STR_APPEND1(res, nusr);
		}
		free(snusrs);
	} else if (flags & F_MOD_REM) {

		snusrs = nusrs = util_safe_strdup(nlist);
		for (i = 0; (nusr = strsep(&nusrs, sep)) != NULL; i++) {
			if (*nusr == '\0')
				continue;
			sn1usrs = n1usrs = util_safe_strdup(nlist);
			for (j = 0; (n1usr = strsep(&n1usrs, sep)) != NULL;
			    j++) {
				if (i != j && strcmp(nusr, n1usr) == 0) {
					util_add_errmsg(errlst, gettext(
					    "Duplicate %s name \"%s\""),
					    usrgrp, nusr);
					free(sn1usrs);
					free(snusrs);
					goto out;
				}
			}
			free(sn1usrs);

			seusrs = eusrs = util_safe_strdup(*elist);
			while ((eusr = strsep(&eusrs, sep)) != NULL) {
				if (strcmp(nusr, eusr) == 0) {
					break;
				}
			}
			free(seusrs);

			if (eusr == NULL) {
				util_add_errmsg(errlst, gettext(
				    "Project does not contain %s name \"%s\""),
				    usrgrp, nusr);
				free(snusrs);
				goto out;
			}
		}
		free(snusrs);


		res = util_safe_zmalloc(1);
		seusrs = eusrs = util_safe_strdup(*elist);
		while ((eusr = strsep(&eusrs, sep)) != NULL) {
			if (*eusr == '\0')
				continue;
			snusrs = nusrs = util_safe_strdup(nlist);
			while ((nusr = strsep(&nusrs, sep)) != NULL) {
				if (strcmp(eusr, nusr) == 0) {
					break;
				}
			}
			free(snusrs);

			if (nusr == NULL) {
				if (*res != '\0')
					res = UTIL_STR_APPEND1(res, ",");
				res = UTIL_STR_APPEND1(res, eusr);
			}
		}
		free(seusrs);
	} else if (flags & F_MOD_SUB || flags & F_MOD_REP) {
		res = util_safe_strdup(nlist);
	}

out:
	if (res != NULL) {
		free(*elist);
		*elist = res;
	}
}

char *
projent_parse_users(char *nlist, int flags, list_t *errlst)
{
	char *ulist = NULL;
	char *susrs, *usrs, *usr;
	regex_t usernexp;
	char *sep;

	if (regcomp(&usernexp, USERN_EXP, REG_EXTENDED) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Failed to compile regular expression: \"%s\""),
		    USERN_EXP);
		goto out;
	}

	sep = (flags & F_PAR_SPC) ? " ," : ",";
	susrs = usrs = util_safe_strdup(nlist);
	ulist = util_safe_zmalloc(1);

	while ((usr = strsep(&usrs, sep)) != NULL) {
		if (*usr == '\0')
			continue;

		if (regexec(&usernexp, usr, 0, NULL, 0) != 0 &&
		    strcmp(usr, "*") != 0 &&
		    strcmp(usr, "!*") != 0) {
			util_add_errmsg(errlst, gettext(
			    "Invalid user name \"%s\""), usr);
			UTIL_FREE_SNULL(ulist);
			break;
		}
		/* Append ',' first if required */
		if (*ulist != '\0')
			ulist = UTIL_STR_APPEND1(ulist, ",");
		ulist = UTIL_STR_APPEND1(ulist, usr);
	}

	free(susrs);
	regfree(&usernexp);
out:
	return (ulist);
}


char *
projent_parse_groups(char *nlist, int flags, list_t *errlst)
{
	char *glist = NULL;
	char *sgrps, *grps, *grp;
	regex_t groupnexp;
	char *sep;

	if (regcomp(&groupnexp, GRUPN_EXP, REG_EXTENDED) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Failed to compile regular expression: \"%s\""),
		    GRUPN_EXP);
		goto out;
	}

	sep = (flags & F_PAR_SPC) ? " ," : ",";
	sgrps = grps = util_safe_strdup(nlist);
	glist = util_safe_zmalloc(1);

	while ((grp = strsep(&grps, sep)) != NULL) {
		if (*grp == '\0')
			continue;

		if (regexec(&groupnexp, grp, 0, NULL, 0) != 0 &&
		    strcmp(grp, "*") != 0 &&
		    strcmp(grp, "!*") != 0) {
			util_add_errmsg(errlst, gettext(
			    "Invalid group name \"%s\""), grp);
			UTIL_FREE_SNULL(glist);
			break;
		}
		/* Append ',' first if required */
		if (*glist != '\0')
			glist = UTIL_STR_APPEND1(glist, ",");
		glist = UTIL_STR_APPEND1(glist, grp);
	}

	free(sgrps);
	regfree(&groupnexp);
out:
	return (glist);
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
projent_validate_unique_id(list_t *plist, projid_t projid, list_t *errlst)
{
	projent_t *ent;
	for (ent = list_head(plist); ent != NULL; ent = list_next(plist, ent)) {
		if (ent->projid == projid) {
			util_add_errmsg(errlst, gettext(
			    "Duplicate projid \"%d\""), projid);
			return (1);
		}
	}
	return (0);
}
int
projent_validate_projid(projid_t projid, int flags, list_t *errlst)
{
	projid_t maxprojid;

	maxprojid = (flags & F_PAR_RES) ? 0 : 100;

	if (projid < maxprojid) {
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
	long long llid;
	regex_t prjidexp;
	int ret = 0;

	if (regcomp(&prjidexp, PRJID_EXP, REG_EXTENDED) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Failed to compile regular expression: \"%s\""), PRJID_EXP);
		return (1);
	}


	if (regexec(&prjidexp, projidstr, 0, NULL, 0) != 0) {
		util_add_errmsg(errlst, gettext("Invalid project id: \"%s\""),
		    projidstr);
		ret = 1;
		goto out;
	}

	llid = strtoll(projidstr, &ptr, 10);

	/* projid should be a positive number */
	if (llid == 0 && errno == ERANGE && *ptr != '\0') {
		util_add_errmsg(errlst, gettext("Invalid project id: \"%s\""),
		    projidstr);
		ret = 1;
		goto out;
	}

	/* projid should be a positive number >= 0 */
	if (llid < 0) {
		util_add_errmsg(errlst, gettext(
		    "Invalid projid \"%lld\": must be >= 0"), llid);
		ret = 1;
		goto out;
	}

	/* projid should be less than UID_MAX */
	if (llid > INT_MAX) {
		util_add_errmsg(errlst, gettext(
		    "Invalid projid \"%lld\": must be <= %d"),
		    llid, INT_MAX);
		ret = 1;
		goto out;
	}

	if (pprojid != NULL)
		*pprojid = llid;
out:
	regfree(&prjidexp);
	return (ret);
}

int
projent_validate_unique_name(list_t *plist, char *pname, list_t *errlst)
{
	projent_t *ent;
	for (ent = list_head(plist); ent != NULL; ent = list_next(plist, ent)) {
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

void
projent_free(projent_t *ent)
{
	free(ent->projname);
	free(ent->comment);
	free(ent->userlist);
	free(ent->grouplist);
	attrib_free_lst(ent->attrs);
	free(ent->attrs);
}

projent_t *
projent_parse_components(char *projname, char *idstr, char *comment,
    char *users, char *groups, char *attr, int flags, list_t *errlst)
{
	projent_t *ent;
	int reterr = 0;

	ent = util_safe_zmalloc(sizeof (projent_t));


	ent->projname = util_safe_strdup(projname);
	ent->comment = util_safe_strdup(comment);

	reterr += projent_parse_name(ent->projname, errlst);
	reterr += projent_parse_projid(idstr, &ent->projid, errlst);
	reterr += projent_parse_comment(ent->comment, errlst);
	ent->userlist = projent_parse_users(users, flags, errlst);
	ent->grouplist = projent_parse_groups(groups, flags, errlst);
	ent->attrs = projent_parse_attributes(attr, flags, errlst);

	if (reterr > 0 || ent->userlist == NULL ||
	    ent->grouplist == NULL || ent->attrs == NULL) {
		projent_free(ent);
		UTIL_FREE_SNULL(ent);
	}

	return (ent);
}
projent_t *
projent_parse(char *projstr, int flags, list_t *errlst)
{
	char *str, *sstr;
	char *projname, *idstr, *comment, *users, *groups, *attrstr;
	projent_t *ent;

	if (projstr == NULL)
		return (NULL);

	projname = idstr = comment = users = groups = attrstr = NULL;
	ent = NULL;

	sstr  = str = util_safe_strdup(projstr);

	if ((projname = util_safe_strdup(strsep(&str, ":"))) == NULL ||
	    (idstr = util_safe_strdup(strsep(&str, ":"))) == NULL ||
	    (comment = util_safe_strdup(strsep(&str, ":"))) == NULL ||
	    (users = util_safe_strdup(strsep(&str, ":"))) == NULL ||
	    (groups = util_safe_strdup(strsep(&str, ":"))) == NULL ||
	    (attrstr = util_safe_strdup(strsep(&str, ":"))) == NULL ||
	    strsep(&str, ":") != NULL) {
		util_add_errmsg(errlst, gettext(
		    "Incorrect number of fields.  Should have 5 \":\"'s."));
		goto out;
	}

	ent = projent_parse_components(projname, idstr, comment, users, groups,
	    attrstr, flags, errlst);
out:
	free(sstr);
	free(projname); free(idstr); free(comment);
	free(users); free(groups); free(attrstr);
	return (ent);
}


void
projent_free_list(list_t *plist)
{
	projent_t *ent;

	if (plist == NULL)
		return;

	while ((ent = list_head(plist)) != NULL) {
		list_remove(plist, ent);
		projent_free(ent);
		free(ent);
	}
}


void
projent_put_list(char *projfile, list_t *plist, list_t *errlst)
{
	char *tmpprojfile, *attrs;
	FILE *fp;
	projent_t *ent;
	struct stat statbuf;
	int ret;

	tmpprojfile = NULL;
	if (asprintf(&tmpprojfile, "%s.%ld_tmp", projfile, getpid()) == -1) {
		util_add_errmsg(errlst, gettext(
		    "Failed to allocate memory"));
		goto out;
	}

	if (stat(projfile, &statbuf) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Failed to access %s: %s"),
		    projfile, strerror(errno));
		goto out;
	}
	if ((fp = fopen(tmpprojfile, "wx")) == NULL) {
		util_add_errmsg(errlst, gettext(
		    "Cannot create %s: %s"),
		    tmpprojfile, strerror(errno));
		goto out;
	}

	for (ent = list_head(plist); ent != NULL; ent = list_next(plist, ent)) {
		if ((attrs = attrib_lst_tostring(ent->attrs)) == NULL) {
			util_add_errmsg(errlst, gettext(
			    "Failed to write to  %s: error allocating memory"),
			    tmpprojfile);
			(void) unlink(tmpprojfile);
			goto out1;
		}
		ret = fprintf(fp, "%s:%ld:%s:%s:%s:%s\n", ent->projname,
		    ent->projid, ent->comment, ent->userlist, ent->grouplist,
		    attrs);
		free(attrs);
		if (ret < 0) {
			util_add_errmsg(errlst, gettext(
			    "Failed to write to  %s: %s"),
			    tmpprojfile, strerror(errno));
			/* Remove the temporary file and exit */
			(void) unlink(tmpprojfile);
			goto out1;
		}
	}

	if (chown(tmpprojfile, statbuf.st_uid, statbuf.st_gid) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Cannot set ownership of  %s: %s"),
		    tmpprojfile, strerror(errno));
		(void) unlink(tmpprojfile);
		goto out1;
	}

	if (rename(tmpprojfile, projfile) != 0) {
		util_add_errmsg(errlst, gettext(
		    "Cannot rename %s to %s : %s"),
		    tmpprojfile, projfile, strerror(errno));
		(void) unlink(tmpprojfile);
		goto out1;
	}

out1:
	(void) fclose(fp);
out:
	free(tmpprojfile);
}

list_t *
projent_get_list(char *projfile, int flags, list_t *errlst)
{
	FILE *fp;
	list_t *plist;
	int line = 0;
	char *buf = NULL, *nlp;
	size_t cap = 0;
	projent_t *ent;

	plist = util_safe_malloc(sizeof (list_t));
	list_create(plist, sizeof (projent_t), offsetof(projent_t, next));

	if ((fp = fopen(projfile, "r")) == NULL) {
		if (errno == ENOENT) {
			/*
			 * There is no project file,
			 * return an empty list.
			 */
			return (plist);
		} else {
			/* Report the error unable to open the file */
			util_add_errmsg(errlst, gettext(
			    "Cannot open %s: %s"),
			    projfile, strerror(errno));

			list_destroy(plist);
			free(plist);
			return (NULL);
		}
	}

	while ((getline(&buf, &cap, fp)) != -1 && ++line) {

		if ((nlp = strchr(buf, '\n')) != NULL)
			*nlp = '\0';

		if ((ent = projent_parse(buf, flags, errlst)) != NULL) {
			list_insert_tail(plist, ent);
		} else {
			/* Report the error */
			util_add_errmsg(errlst, gettext(
			    "Error parsing: %s line: %d: \"%s\""),
			    projfile, line, buf);

			/* free the allocated resources */
			projent_free_list(plist);
			list_destroy(plist);
			UTIL_FREE_SNULL(plist);
			goto out;
		}
	}

	if ((flags & F_PAR_VLD) && (plist != NULL)) {
		if (projent_validate_list(plist, flags, errlst) != 0) {
			projent_free_list(plist);
			list_destroy(plist);
			UTIL_FREE_SNULL(plist);
		}
	}

out:
	(void) fclose(fp);
	free(buf);
	return (plist);
}
