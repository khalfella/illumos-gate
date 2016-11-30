/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <locale.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/task.h>

#include <sys/debug.h>


#include "projent.h"
#include "util.h"

#define SEQU(str1, str2)                (strcmp(str1, str2) == 0)

#define	CHECK_ERRORS_FREE_PLST(errlst, plst, attrs, ecode) {	\
	if (!lst_is_empty(errlst)) {					\
		util_print_errmsgs(errlst);				\
		if (plst != NULL) {					\
			projent_free_lst(plst);				\
			free(plst);					\
		}							\
		free(attrs);						\
		usage();						\
		exit(ecode);						\
	}								\
}


/*
 * Print usage
 */
static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "Usage:\n"
	    "projmod [-n] [-A|-f filename] [-p projid [-o]] [-c comment]\n"
	    "        [-a|-s|-r] [-U user[,user...]] [-G group[,group...]]\n"
	    "        [-K name[=value[,value...]]] [-l new_projectname]\n"
	    "        project\n"));
}


/*
 * main()
 */
int
main(int argc, char **argv)
{
	int e, c, error;

	extern char *optarg;
	extern int optind, optopt;
	lst_t *plst = NULL;
	int flags = 0;
	projent_t *ent, *modent;

	/* Command line options */
	boolean_t fflag, nflag, cflag, oflag, pflag, lflag;
	boolean_t sflag, rflag, aflag;
	boolean_t Uflag, Gflag, Kflag, Aflag;
	boolean_t modify;

	/* Project entry fields */
	char *pname = NULL;
	char *npname = NULL;
	/*projid_t projid; */
	char *projidstr = "";
	char *comment = "";
	char *users = "", *groups = "" , *attrs;
	char *pusers, *pgroups;

	lst_t *pattribs;

	lst_t errlst;

	/* Project file defaults to system project file "/etc/project" */
	char *projfile = PROJF_PATH;
	struct project proj, *projp;
	char buf[PROJECT_BUFSZ];
	char *str;

	fflag = nflag = cflag = oflag = pflag = lflag = B_FALSE;
	sflag = rflag = aflag = B_FALSE;
	Uflag = Gflag = Kflag = Aflag = B_FALSE;

	modify = B_FALSE;



	attrs = util_safe_zmalloc(1);
	lst_create(&errlst);


	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define TEXT_DOMAIN "SYS_TEST"		/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* Parse the command line argument list */
	while((c = getopt(argc, argv, ":hf:nc:op:l:sraU:G:K:A")) != EOF)
		switch(c) {
			case 'h':
				usage();
				exit(0);
				break;
			case 'f':
				fflag = B_TRUE;
				projfile = optarg;
				break;
			case 'n':
				nflag = B_TRUE;
				break;
			case 'c':
				cflag = B_TRUE;
				comment = optarg;
				break;
			case 'o':
				oflag = B_TRUE;
				break;
			case 'p':
				pflag = B_TRUE;
				projidstr = optarg;
				break;
			case 'l':
				lflag = B_TRUE;
				npname = optarg;
				break;
			case 's':
				sflag = B_TRUE;
				break;
			case 'r':
				rflag = B_TRUE;
				break;
			case 'a':
				aflag = B_TRUE;
				break;
			case 'U':
				Uflag = B_TRUE;
				users = optarg;
				break;
			case 'G':
				Gflag = B_TRUE;
				groups = optarg;
				break;
			case 'K':
				Kflag = B_TRUE;
				attrs = UTIL_STR_APPEND2(attrs, ";", optarg);
				break;
			case 'A':
				Aflag = B_TRUE;
				break;
			default:
				util_add_errmsg(&errlst, gettext(
				    "Invalid option: -%c"), optopt);
				break;
		}

	CHECK_ERRORS_FREE_PLST(&errlst, plst, attrs, 2);

	if (optind == argc - 1) {
		pname = argv[optind];
	}

	if (cflag || Gflag || lflag || pflag || Uflag || Kflag || Aflag) {
		modify = B_TRUE;
		if (pname == NULL) {
			util_add_errmsg(&errlst, gettext(
			    "No project name specified"));
		}
	} else if (pname != NULL) {
		util_add_errmsg(&errlst, gettext(
		    "missing -c, -G, -l, -p, -U, or -K"));
	}

	if (Aflag && fflag) {
		util_add_errmsg(&errlst, gettext(
		    "-A and -f are mutually exclusive"));
	}

	if (oflag && !pflag) {
		util_add_errmsg(&errlst, gettext(
		    "-o requires -p projid to be specified"));
	}

	if ((aflag && (rflag || sflag)) || (rflag && (aflag || sflag)) ||
	    (sflag && (aflag || rflag))) {
		util_add_errmsg(&errlst, gettext(
		    "-a, -r, and -s are mutually exclusive"));
	}

	if ((aflag || rflag || sflag) && !(Uflag || Gflag || Kflag)) {
		util_add_errmsg(&errlst, gettext(
		    "-a, -r, and -s require -U users, -G groups "
		    "or -K attributes to be specified"));
	}

	CHECK_ERRORS_FREE_PLST(&errlst, plst, attrs, 2);

	if (aflag) {
		flags |= F_MOD_ADD;
	} else if (rflag) {
		flags |= F_MOD_REM;
	} else if (sflag) {
		flags |= F_MOD_SUB;
	} else {
		flags |= F_MOD_REP;
	}
	if (!nflag) {
		flags |= F_PAR_VLD;
	}
	flags |= F_PAR_RES | F_PAR_DUP;

	plst = projent_get_lst(projfile, flags, &errlst);
	CHECK_ERRORS_FREE_PLST(&errlst, plst, attrs, 2);

	modent = NULL;
	if (pname != NULL) {
		for (e = 0; e < lst_numelements(plst); e++) {
			ent = lst_at(plst, e);
			if (SEQU(ent->projname, pname)) {
				modent = ent;
			}
		}
		if (modent == NULL) {
			util_add_errmsg(&errlst, gettext(
			    "Project \"%s\" does not exist"), pname);
		}
	}

	CHECK_ERRORS_FREE_PLST(&errlst, plst, attrs, 2);

	/*
	 * If there is no modification options, simply reading the file, which
	 * includes parsing and verifying, is sufficient.
	 */
	if (!modify) {
		exit(0);
	}

	VERIFY(modent != NULL);

	if (lflag && projent_parse_name(pname, &errlst) == 0 &&
	    projent_validate_unique_name(plst, npname, &errlst) == 0) {
		free(modent->projname);
		modent->projname = strdup(npname);
	}

	if (cflag && projent_parse_comment(comment, &errlst) == 0) {
		free(modent->comment);
		modent->comment = strdup(comment);
	}

	if (Uflag) {
		pusers = projent_parse_usrgrp("user", users, F_PAR_SPC,
		    &errlst);
		if (pusers != NULL) {
			projent_merge_usrgrp("user", &modent->userlist,
			    pusers, flags, &errlst);
			free(pusers);
		}
	}

	if (Gflag) {
		pgroups = projent_parse_usrgrp("group", groups, F_PAR_SPC,
		    &errlst);
		if (pgroups != NULL) {
			projent_merge_usrgrp("group", &modent->grouplist,
			    pgroups, flags, &errlst);
			free(pgroups);
		}
	}

	if (Kflag) {
		pattribs = projent_parse_attributes(attrs, F_PAR_UNT, &errlst);
		if (pattribs != NULL) {
			projent_merge_attributes(&modent->attrs,
			    pattribs, flags, &errlst);
			projent_sort_attributes(modent->attrs);
			projent_free_attributes(pattribs);
			UTIL_FREE_SNULL(pattribs);
		}
	}

	if (!nflag) {
		(void) projent_validate(modent, flags, &errlst);
	}
	CHECK_ERRORS_FREE_PLST(&errlst, plst, attrs, 2);

	if (modify) {
		projent_put_lst(projfile, plst, &errlst);
	}
	CHECK_ERRORS_FREE_PLST(&errlst, plst, attrs, 2);

	if (Aflag && (error = setproject(pname, "root",
	    TASK_FINAL|TASK_PROJ_PURGE)) != 0) {
		if (error == SETPROJ_ERR_TASK) {
			if (errno == EAGAIN) {
				util_add_errmsg(&errlst, gettext(
				    "resource control limit has been reached"));
			} else if (errno == ESRCH) {
				util_add_errmsg(&errlst, gettext(
				    "user \"%s\" is not a member "
				    "of project \"%s\""), "root", pname);
			} else {
				util_add_errmsg(&errlst, gettext(
				    "could not join project \"%s\""), pname);
			}
		} else if (error == SETPROJ_ERR_POOL) {
			if (errno == EACCES) {
				util_add_errmsg(&errlst, gettext(
				    "no resource pool accepting default "
				    "bindings exists for project \"%s\""),
				    pname);
			} else if (errno == ESRCH) {
				util_add_errmsg(&errlst, gettext(
				    "specified resource pool does not exist "
				    "for project \"%s\""), pname);
			} else {
				util_add_errmsg(&errlst, gettext(
				    "could not bind to default resource pool "
				    "for project \"%s\""), pname);
			}
		} else {
			/*
			 * error represents the position - within the
			 * semi-colon delimited attribute - that generated
			 * the error.
			 */
			if (error <= 0) {
				util_add_errmsg(&errlst, gettext(
				    "setproject failed for project \"%s\""),
				    pname);
			} else {
				/* To be completed */
				projp = getprojbyname(pname, &proj, buf,
				    sizeof(buf));
				pattribs = (projp != NULL) ?
				    projent_parse_attributes(projp->pj_attr,
				    0, &errlst) : NULL;
				if (projp != NULL && pattribs != NULL &&
				    (str = projent_attrib_tostring(
				    lst_at(pattribs, error - 1))) != NULL) {
					util_add_errmsg(&errlst, gettext(
					    "warning, \"%s\" resource control "
					    "assignment failed for project "
					    "\"%s\""), str, pname);
					free(str);
				} else {
					util_add_errmsg(&errlst, gettext(
					    "warning, resource control "
					    "assignment failed for project "
					    "\"%s\" attribute %d"), pname,
					    error);
				}

				if (pattribs != NULL) {
					projent_free_attributes(pattribs);
					UTIL_FREE_SNULL(pattribs);
				}
			}
		}
	}

	CHECK_ERRORS_FREE_PLST(&errlst, plst, attrs, 2);

	return (0);
}
