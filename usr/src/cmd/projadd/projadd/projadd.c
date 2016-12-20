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


#define	CHECK_ERRORS_FREE_PLIST(errlst, plist, attrs, ecode) {	\
	if (!list_is_empty(errlst)) {					\
		util_print_errmsgs(errlst);				\
		list_destroy(errlst);					\
		if (plist != NULL) {					\
			projent_free_list(plist);			\
			list_destroy(plist);				\
			free(plist);					\
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
	int c;
	extern char *optarg;
	extern int optind, optopt;
	projid_t maxpjid = 99;
	list_t *plist = NULL;
	int flags = 0;
	char *projfile = PROJF_PATH;

	projent_t *ent;
	boolean_t nflag, pflag, oflag;		/* Command line flags.	*/
	char *pname, *projidstr, *comment;	/* projent fields.	*/
	char *users, *groups, *attrs;
	projid_t projid;

	list_t errlst;

	/* Project file defaults to system project file "/etc/project" */
	users = groups = comment = projidstr = "";

	nflag = pflag = oflag = B_FALSE;
	attrs = util_safe_zmalloc(1);
	util_init_errlst(&errlst);


	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"		/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* Parse the command line argument list */
	while ((c = getopt(argc, argv, ":hnf:p:oc:U:G:K:")) != EOF)
		switch (c) {
			case 'h':
				usage();
				exit(0);
				break;
			case 'n':
				nflag = B_TRUE;
				break;
			case 'f':
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
				comment = optarg;
				break;
			case 'U':
				users = optarg;
				break;
			case 'G':
				groups = optarg;
				break;
			case 'K':
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

	if (!nflag)
		flags |= F_PAR_VLD;
	flags |= F_PAR_RES | F_PAR_DUP;

	/* Parse the project file to get the list of the projects */
	plist = projent_get_list(projfile, flags, &errlst);
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);


	/* Parse and validate new project id */
	if (pflag && projent_parse_projid(projidstr, &projid, &errlst) == 0) {
		if (!nflag) {
			projent_validate_projid(projid, 0, &errlst);
			if (!oflag) {
				projent_validate_unique_id(plist, projid,
				    &errlst);
			}
		}
	}
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Find the maxprojid */
	for (ent = list_head(plist); ent != NULL; ent = list_next(plist, ent))
		maxpjid = (ent->projid > maxpjid) ? ent->projid : maxpjid;


	if (!pflag && asprintf(&projidstr, "%ld", maxpjid + 1) == -1) {
		util_add_errmsg(&errlst, gettext("Failed to allocate memory"));
		CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);
	}

	pname = argv[optind];
	ent = projent_parse_components(pname, projidstr, comment, users,
	    groups, attrs, F_PAR_SPC | F_PAR_UNT, &errlst);
	if (!pflag)
		free(projidstr);

	if (!nflag)
		projent_validate_unique_name(plist, pname, &errlst);

	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Sort attributes list */
	projent_sort_attributes(ent->attrs);

	/* Add the new project entry to the list */
	list_insert_tail(plist, ent);

	/* Validate the projent before writing the list to the project file */
	(void) projent_validate(ent, flags, &errlst);
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	/* Write out the project file */
	projent_put_list(projfile, plist, &errlst);
	CHECK_ERRORS_FREE_PLIST(&errlst, plist, attrs, 2);

	return (0);
}
