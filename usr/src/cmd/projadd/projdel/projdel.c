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

#define	SEQU(str1, str2)		(strcmp(str1, str2) == 0)

/*
 * Print usage
 */
static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "Usage:\n"
	    "projdel [-f filename] project\n"));
}

/*
 * main()
 */
int
main(int argc, char **argv)
{
	int e, c, ret = 0;
	int flags;
	extern char *optarg;
	extern int optind, optopt;
	lst_t *plst;			/* Projects list */
	projent_t *ent, *delent;
	int del;
	boolean_t fflag = B_FALSE;	/* Command line flag */
	char *pname;			/* Project name */
	list_t errlst;			/* Errors list */
	char *projfile = PROJF_PATH;	/* Project file "/etc/project" */

	list_create(&errlst, sizeof (errmsg_t), offsetof(errmsg_t, next));


	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"		/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* Parse the command line argument list */
	while ((c = getopt(argc, argv, ":hf:")) != EOF)
		switch (c) {
			case 'h':
				usage();
				exit(0);
				break;
			case 'f':
				fflag = B_TRUE;
				projfile = optarg;
				break;
			default:
				util_add_errmsg(&errlst, gettext(
				    "Invalid option: -%c"), optopt);
				break;
		}

	if (optind != argc -1) {
		fprintf(stderr, "No project name specified\n");
		exit(2);
	}

	/* Name of the project to delete */
	pname = argv[optind];

	flags = F_PAR_VLD | F_PAR_RES | F_PAR_DUP;

	/* Parse the project file to get the list of the projects */
	plst = projent_get_lst(projfile, flags, &errlst);

	if (!list_is_empty(&errlst)) {
		util_print_errmsgs(&errlst);
		list_destroy(&errlst);
		usage();
		exit(2);
	}

	/* Find the project to be deleted */
	del = 0;
	for (e = 0; e < lst_numelements(plst); e++) {
		ent = lst_at(plst, e);
		if (SEQU(ent->projname, pname)) {
			del++;
			delent = ent;
		}
	}

	if (del == 0) {
		fprintf(stderr, "Project \"%s\" does not exist\n", pname);
		usage();
		ret = 2;
		goto out;
	} else if (del > 1) {
		fprintf(stderr, "Duplicate project name \"%s\"", pname);
		usage();
		ret = 2;
		goto out;
	}

	/* Remove the project entry from the list */
	lst_remove(plst, delent);

	/* Write out the project file */
	projent_put_lst(projfile, plst, &errlst);

	if (!list_is_empty(&errlst)) {
		util_print_errmsgs(&errlst);
		usage();
		ret = 2;
	}
out:
	projent_free_lst(plst);
	free(plst);
	list_destroy(&errlst);
	return (ret);
}
