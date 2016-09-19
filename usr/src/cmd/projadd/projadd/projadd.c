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

#include "projent.h"
#include "util.h"


#define	CHECK_ERRORS_FREE_PLIST(perrlst, pplist, attrs, ecode) {	\
	if (!list_is_empty(perrlst)) {					\
		util_print_errmsgs(perrlst);				\
		list_destroy(perrlst);					\
		if (pplist != NULL) {					\
			projent_free_list(pplist);			\
			list_destroy(pplist);				\
			free(pplist);					\
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
	    "projadd [-n] [-f filename] [-p projid [-o]] [-c comment]\n"
	    "        [-U user[,user...]] [-G group[,group...]]\n"
	    "        [-K name[=value[,value...]]] project\n"));
}

/*
 * main()
 */
int
main(int argc, char **argv)
{
	int c, ret = 0;

	extern char *optarg;
	extern int optind, optopt;
	projid_t maxpjid = 99;
	list_t *plist = NULL;
	projent_t *ent;
	char *str;

	/* Command line options */
	boolean_t nflag, fflag, pflag, oflag, cflag, Uflag, Gflag, Kflag;

	/* Project entry fields */
	char *pname;
	projid_t projid;
	char *projidstr = "";
	char *comment = "";
	char *users = "", *groups = "" , *attrs;

	list_t errlst;

	/* Project file defaults to system project file "/etc/project" */
	char *projfile = PROJF_PATH;

	nflag = fflag = pflag = oflag = B_FALSE;
	cflag = Uflag = Gflag = Kflag = B_FALSE;
	attrs = util_safe_zmalloc(1);
	list_create(&errlst, sizeof(errmsg_t), offsetof(errmsg_t, next));


	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define TEXT_DOMAIN "SYS_TEST"		/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* Parse the command line argument list */
	while((c = getopt(argc, argv, ":hnf:p:oc:U:G:K:")) != EOF)
		switch(c) {
			case 'h':
				usage();
				exit(0);
				break;
			case 'n':
				nflag = B_TRUE;
				break;
			case 'f':
				fflag = B_TRUE;
				projfile = optarg;
				break;
			case 'p':
				pflag = B_TRUE;
				projidstr = optarg;
				break;
			case 'o':
				oflag = B_TRUE;
				break;
			case 'c':
				cflag = B_TRUE;
				comment = optarg;
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
			default:
				util_add_errmsg(&errlst, gettext(
				    "Invalid option: -%c"), optopt);
				break;
		}



	if (oflag && !pflag) {
		util_add_errmsg(&errlst, gettext(
		    "-o requires -p projid to be specified"));
	}

	if (optind != argc -1) {
		util_add_errmsg(&errlst, gettext("No project name specified"));
	}

	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Parse the project file to get the list of the projects */
	plist = projent_get_list(projfile, &errlst);
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Parse and validate new project name */
	pname = argv[optind];
	if (projent_parse_name(pname, &errlst) == 0 && !nflag)
		projent_validate_unique_name(plist, pname, &errlst);
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Parse and validate new project id */
	if (pflag && projent_parse_projid(projidstr, &projid, &errlst) == 0) {
		if (!nflag) {
			projent_validate_projid(projid, &errlst);
			if (!oflag) {
				projent_validate_unique_id(plist, projid,
				    &errlst);
			} 
		}
	}
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Find the maxprojid */
	for(ent = list_head(plist); ent != NULL; ent = list_next(plist, ent))
		maxpjid = (ent->projid > maxpjid) ? ent->projid : maxpjid;

	
	if (!pflag && asprintf(&projidstr, "%ld", maxpjid + 1) == -1) {
		util_add_errmsg(&errlst, gettext("Failed to allocate memory"));
		CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);
	}

	ent = projent_parse_components(pname, projidstr, comment, users,
	    groups, attrs, &errlst);

	if (!pflag)
		free(projidstr);

	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Sort attributes list */
	projent_sort_attributes(ent->attrs);


	/* Add the new project entry to the list */
	list_insert_tail(plist, ent);

	/* Validate the projent before writing the list to the project file */
	(void) projent_validate(ent, &errlst);
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Write out the project file */
	projent_put_list(projfile, plist, &errlst);
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	return (0);
}
