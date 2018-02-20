/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2018 <contributor>
 */

/*
 * A small program to test/debug upci driver.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/stat.h>


#include <sys/upci.h>

#define UPCI_DEV	"/devices/pci@0,0/pci15ad,1976@0:upci"
#define UPCI_MAX_DEVS	10


/* Flags */
static boolean_t g_lflag = B_FALSE;
static boolean_t g_vflag = B_FALSE;



static int g_fd = -1;

static int
usage(int status)
{
	fprintf(stderr, "Usage upci: [-l] [-h] [-v]\n"
	    "\t-h show this help message\n"
	    "\t-l list PCI devices controlled by upci\n"
	    "\t-v verbose output\n");
	exit(status);
}

static int
upci_list_devices()
{
	int i, devcnt;
	upci_cmd_t cmd;
	upci_devinfo_t *di;

	if ((di = malloc(sizeof (upci_devinfo_t) * UPCI_MAX_DEVS)) == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		return (1);
	}

	cmd.cm_uobufsz = sizeof (upci_devinfo_t) * UPCI_MAX_DEVS;
	cmd.cm_uobuff = (uint64_t)((intptr_t) di);

	if (ioctl(g_fd, UPCI_IOCTL_LIST_DEVICES, &cmd) != 0) {
		fprintf(stderr, "upci driver LIST_DEVICES ioctl failed\n");
		return (1);
	}

	devcnt = cmd.cm_uobufsz / sizeof (upci_devinfo_t);
	printf("%ld devices to list:\n", devcnt);

	for (i = 0; i < devcnt; i++) {

		if (!g_vflag) {
			printf("dev[%d]: di_devpath = \"%-40s\" flags = %x\n",
			    i, di[i].di_devpath, di[i].di_flags);
			continue;
		}

		/* Verbose */
		printf("dev[%d]:\n", i);
		printf("    di_devpath = \"%s\"\n", di[i].di_devpath);
		printf("    di_flags = %x\n", di[i].di_flags);
	}

	free(di);
	return (0);
}

int
main(int argc, char **argv)
{
	int c;

	while((c = getopt(argc, argv, "lhv")) != EOF) {
		switch (c) {
		case 'l':
			g_lflag = B_TRUE;
		break;
		case 'h':
			return usage(0);
		break;
		case 'v':
			g_vflag = B_TRUE;
		break;
		default:
			return usage(1);
		break;
		}
	}

	if((g_fd = open(UPCI_DEV, O_RDONLY)) == -1) {
		fprintf(stderr, "Failed to open upci device \"%s\"\n",
		    UPCI_DEV);
		return (1);
	}

	/* Make sure the user has choosen acceptable flags combination */

	if (g_lflag) {
		return upci_list_devices();
	}

	return usage(1);
}
