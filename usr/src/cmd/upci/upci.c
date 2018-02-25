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
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stropts.h>
#include <libdevinfo.h>
#include <sys/types.h>
#include <sys/mkdev.h>
#include <sys/stat.h>


#include <sys/upci.h>

#define UPCI_DEV	"/devices/pci@0,0/pci15ad,7a0@15/pci15ad,7b0@0:upci"
#define UPCI_MAX_DEVS	10


/* Flags */
static boolean_t g_lflag = B_FALSE;
static boolean_t g_vflag = B_FALSE;
static boolean_t g_dflag = B_FALSE;
static boolean_t g_oflag = B_FALSE;
static boolean_t g_cflag = B_FALSE;
static boolean_t g_rflag = B_FALSE;
static boolean_t g_wflag = B_FALSE;



static int g_fd = -1;

static int
usage(int status)
{
	fprintf(stderr, "Usage upci: -l [-v] -d devpath\n"
	    "\t-o -d devpath\n"
	    "\t-c -d devpath\n"
	    "\t-r [region:]offset:length -d devpath\n"
	    "\t-w [region:]offset:length:data -d devpath\n"
	    "\t\n"
	    "\t-l list PCI devices controlled by upci\n"
	    "\t-o open a pci device\n"
	    "\t-c close a pci device\n"
	    "\t-r read data out of a pci device\n"
	    "\t-w write data to a pci device\n"
	    "\t-v verbose output\n");
	exit(status);
}

/*
static void *
upci_malloc(size_t sz)
{
	void *ptr;
	if ((ptr = malloc(sz)) == NULL) {
		fprintf(stderr, "Error: Failed to allocate memory\n");
		exit(1);
	}
	return (ptr);
}
*/
static void *
upci_strdup(char *s1)
{
	char *s2;
	if ((s2 = strdup(s1)) == NULL) {
		fprintf(stderr, "Error: Failed to allocate memory\n");
		exit(1);
	}
	return (s1);
}

static int
upci_is_hex(char *s)
{
	 while ((*s = tolower(*s))) {
		if (!isdigit(*s) && !(*s >= 'a' && *s <= 'f'))
			return (0);
		s++;
	}
	return (*s == '\0');
}

static int
upci_parse_command(char *rwcom, int *oreg, uint64_t *ooff,
    char *olen, char *odata)
{
	int rval = 1;
	char *sstr, *str, *tmp;
	char *reg, *off, *len, *data;
	char *c;

	/* Copy the command string */
	sstr = str = upci_strdup(rwcom);

	if ((off = strsep(&str, ":")) == NULL) goto out;

	/* By default set region to -1 */
	*oreg = -1;
	if ((tmp = strchr(off, '-')) != NULL) {
		*tmp++ = '\0';
		reg = off;
		off = tmp;

		/* Check and output regeion */
		if (*reg == '\0' ||
		    strlen(reg) > 2 ||
		    !upci_is_hex(reg)) {
			goto out;
		}
		*oreg = strtol(reg, NULL, 16);
	}

	/* Check and output offset */
	if (*off == '\0' ||
	    strlen(off) > 16 ||
	    !upci_is_hex(off)) {
		goto out;
	}
	*ooff = strtoll(off, NULL, 16);


	/* Extract check and output length */
	if ((len = strsep(&str, ":")) == NULL ||
	    *len == '\0' ||
	    strlen(len) > 1 ||
	    !upci_is_hex(len)) {
		goto out;
	}
	*olen = strtol(len, NULL, 16);



	/* Validate length in {1, 2, 4, 8} */
	if (*olen != 1 &&
	    *olen != 2 &&
	    *olen != 4 &&
	    *olen != 8) {
		goto out;
	}

	/* In case we are writing data */
	if (g_wflag) {

		/* Validate and check data */
		if ((data = strsep(&str, ":")) == NULL ||
		    *data == '\0' ||
		    strlen(data) > 16 ||
		    !upci_is_hex(data) ||
		    (*olen * 2) != strlen(data)) {
			goto out;
		}

		/* Convert text hex to bin */
		c = data;
		while (*c) {
			if (isdigit(*c))
				*odata = *c - '0';
			else
				*odata = 10 + *c - 'a';

			*odata <<= 4; c++;

			if (isdigit(*c))
				*odata += *c - '0';
			else
				*odata += 10 + *c - 'a';

			odata++; c++;
		}
	}

	/* Check for extra fields */
	if (strsep(&str, ":") == NULL)
		rval = 0;

out:
	free(sstr);
	return (rval);
}

static int
upci_rw(char *rwcom, char *devpath)
{
	char len;
	char datain[8], dataout[8];
	int i, reg, ioctlcmd;
	uint64_t off;
	upci_cmd_t cmd;
	upci_cfg_rw_t rw;

	if (upci_parse_command(rwcom, &reg, &off, &len, datain) != 0) {
		fprintf(stderr, "Error: Failed to parse command\n");
		return (1);
	}

	if (reg != -1) {
		fprintf(stderr, "Region I/O is not implemented yet\n");
		return (1);
	}

	strcpy(rw.rw_devpath ,devpath);
	rw.rw_offset = off;
	rw.rw_count = len;

	strcpy(cmd.cm_devpath, devpath);
	cmd.cm_uibuff = (uintptr_t) &rw;
	cmd.cm_uibufsz = sizeof(rw);
	cmd.cm_uobuff = (uintptr_t) (g_wflag ? datain : dataout);
	cmd.cm_uobufsz = len;

	ioctlcmd = g_wflag ? UPCI_IOCTL_CFG_WRITE: UPCI_IOCTL_CFG_READ;

	if (ioctl(g_fd, ioctlcmd, &cmd) != 0) {
		fprintf(stderr, "Error: I/O ioctl "
		    "failed for \"%s\"\n", devpath);
		return (1);
	}


	if (g_rflag) {
		for (i = 0; i < len; i++) {
			printf("%02x", dataout[i] & 0xff);
		}
		putchar('\n');
	}
	
	return (0);
}

static int
upci_open_device(char *devpath)
{
	upci_cmd_t cmd;

	cmd.cm_uibuff = (intptr_t)devpath;
	cmd.cm_uibufsz = strlen(devpath) + 1;
	if (ioctl(g_fd, UPCI_IOCTL_OPEN_DEVICE, &cmd) != 0) {
		fprintf(stderr, "Failed to open \"%s\"\n", devpath);
		return (1);
	}
	return (0);
}

static int
upci_close_device(char *devpath)
{
	upci_cmd_t cmd;

	cmd.cm_uibuff = (intptr_t)devpath;
	cmd.cm_uibufsz = strlen(devpath) + 1;
	if (ioctl(g_fd, UPCI_IOCTL_CLOSE_DEVICE, &cmd) != 0) {
		fprintf(stderr, "Failed to close \"%s\"\n", devpath);
		return (1);
	}
	return (0);
}

static int
upci_list_devices_devinfo()
{
	int instance;
	di_minor_t minor;
	di_node_t root, node;
	char *path;
	dev_t dev;

	if ((root = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
		fprintf(stderr, "Error: failed to initialize libdevinfo\n");
		return (1);
	}

	node = di_drv_first_node("upci", root);
	while (node != DI_NODE_NIL) {

		minor = DI_MINOR_NIL;
		instance = di_instance(node);

		while ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {

			path = di_devfs_minor_path(minor);
			dev = di_minor_devt(minor);
			printf("[%02d %03d:%02d] /devices%s\n", instance,
			    major(dev), minor(dev), path);
			di_devfs_path_free(path);
			node = di_drv_next_node(node);
		}

		node = di_drv_next_node(node);
	}

	return (0);
}

static int
upci_list_devices()
{
	int i, devcnt;
	upci_cmd_t cmd;
	upci_devinfo_t *di;

	return upci_list_devices_devinfo();

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
	char *devpath, *rwcom;

	devpath = rwcom = NULL;
	while((c = getopt(argc, argv, "lvd:ocr:w:h")) != EOF) {
		switch (c) {
		case 'l':
			g_lflag = B_TRUE;
		break;
		case 'v':
			g_vflag = B_TRUE;
		break;
		case 'd':
			g_dflag = B_TRUE;
			devpath = optarg;
		break;
		case 'o':
			g_oflag = B_TRUE;
		break;
		case 'c':
			g_cflag = B_TRUE;
		break;
		case 'r':
			g_rflag = B_TRUE;
			rwcom = optarg;
		break;
		case 'w':
			g_wflag = B_TRUE;
			rwcom = optarg;
		break;
		case 'h':
		default:
			return usage(1);
		break;
		}
	}

	if (g_lflag + g_oflag + g_cflag + g_rflag + g_wflag != 1)
		return usage(1);
	if (!g_dflag)
		return usage(1);

	if((g_fd = open(UPCI_DEV, O_RDONLY)) == -1) {
		fprintf(stderr, "Failed to open upci device \"%s\"\n",
		    UPCI_DEV);
		return (1);
	}

	/* Do the work */
	if (g_lflag) {
		return upci_list_devices();
	} else if (g_oflag) {
		return upci_open_device(devpath);
	} else if (g_cflag) {
		return upci_close_device(devpath);
	} else if (g_rflag || g_wflag) {
			return upci_rw(rwcom, devpath);
	}

	return usage(1);
}
