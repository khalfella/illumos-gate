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



/* Command line options */
static boolean_t g_nflag = B_FALSE;
static boolean_t g_fflag = B_FALSE;
static boolean_t g_pflag = B_FALSE;
static boolean_t g_oflag = B_FALSE;
static boolean_t g_cflag = B_FALSE;
static boolean_t g_Uflag = B_FALSE;
static boolean_t g_Gflag = B_FALSE;
static boolean_t g_Kflag = B_FALSE;



static char *tmpprojfile, *projfile = PROJF_PATH;
static char *pname;

static list_t errlst;

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



int
main(int argc, char **argv)
{
	int c;

	extern char *optarg;
	extern int optind, optopt;
	projid_t projid, maxpjid = 99;
	char *projidstr = "";
	char *comment = "";
	list_t *plist;
	projent_t *ent;
	char *err = NULL;
	char *ptr;
	char *str;

	char *users, *userslist, *groups, *groupslist, *attrslist;
	userslist = groupslist = attrslist = "";

	users = groups = NULL;
	lst_t *attrs = NULL;

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
				g_nflag = B_TRUE;
				break;
			case 'f':
				g_fflag = B_TRUE;
				projfile = optarg;
				break;
			case 'p':
				g_pflag = B_TRUE;
				projidstr = optarg;
				break;
			case 'o':
				g_oflag = B_TRUE;
				break;
			case 'c':
				g_cflag = B_TRUE;
				comment = optarg;
				break;
			case 'U':
				g_Uflag = B_TRUE;
				userslist = optarg;
				break;
			case 'G':
				g_Gflag = B_TRUE;
				groupslist = optarg;
				break;
			case 'K':
				g_Kflag = B_TRUE;
				attrslist = optarg;
				break;
			default:
				util_add_errmsg(&errlst, gettext(
				    "Invalid option: -%c"), optopt);
				break;
		}



	if (g_oflag && !g_pflag) {
		util_add_errmsg(&errlst, gettext(
		    "-o requires -p projid to be specified"));
	}

	if (optind != argc -1) {
		util_add_errmsg(&errlst, gettext(
		    "No project name specified"));
	}

	/* Parse the project file to get the list of the projects */
	plist = projent_get_list(projfile, &errlst);

	if (!list_is_empty(&errlst)) {
		projent_print_errmsgs(&errlst);
		list_destroy(&errlst);
		usage();
		exit(2);
	}

	pname = argv[optind];
	if (projent_parse_name(pname, &errlst) == 0 && !g_nflag)
	    projent_validate_unique_name(plist, pname, &errlst);

	if (g_pflag && projent_parse_projid(projidstr, &projid, &errlst) == 0) {
		if (!g_nflag) {
			projent_validate_projid(projid, &errlst);
			if (!g_oflag) {
				projent_validate_unique_id(plist, projid,
				    &errlst);
			} 
		}

	}


	if (g_cflag)
		(void) projent_parse_comment(comment, &errlst);
	if (g_Uflag)
		users =  projent_parse_usrgrp("user",userslist, &errlst);
	if (g_Gflag)
		groups =  projent_parse_usrgrp("group", groupslist, &errlst);
	if (g_Kflag) {
		attrs = projent_parse_attributes(attrslist, &errlst);
		projent_sort_attributes(attrs);
	}

	if (plist != NULL) {
		for(ent = list_head(plist); ent != NULL;
		    ent = list_next(plist, ent)) {
			maxpjid = (ent->projid > maxpjid) ?
			    ent->projid : maxpjid;
			/* debugging code shoudl be removed eventually */
			projent_print_ent(ent);
		}
	}

/* for testing */
	if (!list_is_empty(&errlst)) {
		projent_print_errmsgs(&errlst);
		list_destroy(&errlst);
		usage();
		exit(2);
	}
/* for testing */


	/* We have all the required components to build this new projent */
	ent = util_safe_zmalloc(sizeof(projent_t));
	list_link_init(&ent->next);

	ent->projname = strdup(pname);
	ent->projid = (g_pflag) ? projid : maxpjid + 1;
	ent->comment = strdup(comment);
	ent->userlist = (users != NULL) ? strdup(users) : strdup("");
	ent->grouplist = (groups != NULL) ? strdup(groups) : strdup("");
	if (attrs &&  (str = projent_attrib_lst_tostring(attrs)) != NULL) {
		ent->attr = str;
	} else {
		ent->attr = strdup("");
	}


	/* Validate the projent before addig it to the list */
	if (projent_validate(ent, attrs, &errlst) != 0 || !list_is_empty(&errlst)) {
		projent_print_errmsgs(&errlst);
		list_destroy(&errlst);
		usage();
		exit(2);
	}


	/* Now we can add it ot the list */
	list_insert_tail(plist, ent);


	/* Write out the project file */


	/* Print some informaotin for debugging */
	printf("projfile = %s\n", projfile);
	printf("maxpjid = %d\n", maxpjid);
	printf("g_Uflag = %d g_Gflag = %d\n", g_Uflag, g_Gflag);

	printf("ent->projname = %s\n", ent->projname);
	printf("ent->projid = %d\n", ent->projid);
	printf("ent->comment = \"%s\"\n", ent->comment);
	printf("ent->userlist = \"%s\"\n", ent->userlist);
	printf("ent->grouplist = \"%s\"\n", ent->grouplist);
	printf("ent->att = \"%s\"\n", ent->attr);

	printf("end of main....\n");


	if (plist != NULL) {
		projent_free_list(plist);
	}
	if (attrs) {
		projent_free_attributes(attrs);
	}

	free(users);
	free(groups);
	free(attrs);

	return (0);
}
