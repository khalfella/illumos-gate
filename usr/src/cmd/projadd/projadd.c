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




#include "projadd.h"



/* Command line options */
static boolean_t g_nflag = B_FALSE;
static boolean_t g_fflag = B_FALSE;
static boolean_t g_pflag = B_FALSE;
static boolean_t g_oflag = B_FALSE;
static boolean_t g_cflag = B_FALSE;
static boolean_t g_Uflag = B_FALSE;
static boolean_t g_Gflag = B_FALSE;
static boolean_t g_Kflag = B_FALSE;


static boolean_t errflg = B_FALSE;

static char *tmpprojfile, *projfile = PROJF_PATH;
static char *pname;
static char *userlist, *grouplist, *nvlist;
static projid_t projid;

static list_t errors;

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
	extern int optind;
	id_t projid, maxpjid = 99;
	list_t *plist;
	projent_t *ent;
	char *err = NULL;
	char *ptr;
	char *errorstr;


	list_create(&errors, sizeof(errmsg_t), offsetof(errmsg_t, next));

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define TEXT_DOMAIN "SYS_TEST"		/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* Parse the command line argument list */
	while((c = getopt(argc, argv, ":hnf:p:ocU:G:K:")) != EOF)
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
				if (((projid = strtol(optarg, &ptr, 10)) == 0 &&
				    errno == EINVAL) || *ptr != '\0' ) {
					asprintf(&errorstr, gettext(
					    "Invalid project id:  %s"), optarg);
					projent_add_errmsg(&errors, errorstr);
					free(errorstr);
					errflg = B_TRUE;
				}
				break;
			case 'o':
				g_oflag = B_TRUE;
				break;
			case 'c':
				g_cflag = B_TRUE;
				break;
			case 'U':
				g_Uflag = B_TRUE;
				userlist = optarg;
				break;
			case 'G':
				g_Uflag = B_TRUE;
				grouplist = optarg;
				break;
			default:
				errflg = B_TRUE;
				break;
		}



	if (g_oflag && !g_pflag) {
		asprintf(&errorstr, gettext(
		    "-o requires -p projid to be specified"));
		projent_add_errmsg(&errors, errorstr);
		free(errorstr);
		errflg = B_TRUE;
	}

	if (optind != argc -1) {
		asprintf(&errorstr, gettext(
		    "No project name specified"));
		projent_add_errmsg(&errors, errorstr);
		free(errorstr);
		errflg = B_TRUE;
	}

	if ((plist = projent_get_list(projfile, &errors)) == NULL) {
		errflg = B_TRUE;

	}

	if (errflg) {
		projent_print_errmsgs(&errors);
		projent_free_errmsgs(&errors);
		list_destroy(&errors);
		usage();
		exit(2);
	}

	pname = argv[optind];

	if (plist != NULL) {
		for(ent = list_head(plist); ent != NULL;
		    ent = list_next(plist, ent)) {
			maxpjid = (ent->projid > maxpjid) ?
			    ent->projid : maxpjid;
			projent_print_ent(ent);
		}
	}
	printf("pname = %s projfile = \n", pname, projfile);
	printf("maxpjid = %d\n", maxpjid);

	printf("end of main....\n");
	return (0);
}
