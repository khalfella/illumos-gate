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
#include <errno.h>
#include <libdevinfo.h>
#include <sys/types.h>
#include <sys/mkdev.h>
#include <sys/stat.h>

#include <sys/upci.h>

/* Flags */
static boolean_t g_lflag = B_FALSE;
static boolean_t g_vflag = B_FALSE;
static boolean_t g_dflag = B_FALSE;
static boolean_t g_oflag = B_FALSE;
static boolean_t g_cflag = B_FALSE;
static boolean_t g_Oflag = B_FALSE;
static boolean_t g_Cflag = B_FALSE;
static boolean_t g_iflag = B_FALSE;
static boolean_t g_rflag = B_FALSE;
static boolean_t g_wflag = B_FALSE;

/* devinfo root node */
static di_node_t g_di_root;

static void
usage()
{
	fprintf(stderr, "Usage upci: -l [-v]\n"
	    "\t-o -d dev\n"
	    "\t-c -d dev\n"
	    "\t-O -d dev\n"
	    "\t-C -d dev\n"
	    "\t-i region -d dev\n"
	    "\t-r [region:]offset:length -d dev\n"
	    "\t-w [region:]offset:length:data -d dev\n"
	    "\t\n"
	    "\t-l list PCI devices controlled by upci\n"
	    "\t-o open a pci device\n"
	    "\t-c close a pci device\n"
	    "\t-O open a pci regions\n"
	    "\t-C close a pci regions\n"
	    "\t-i get dev/region info\n"
	    "\t-r read data out of a pci device\n"
	    "\t-w write data to a pci device\n"
	    "\t-v verbose output\n");
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
	return (s2);
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
upci_open_minor(int dev)
{
	di_minor_t minor;
	di_node_t node;
	char *path;
	char opath[MAXPATHLEN];

	node = di_drv_first_node("upci", g_di_root);
	while (node != DI_NODE_NIL) {

		minor = DI_MINOR_NIL;

		if ((di_instance(node) == dev) &&
		    (minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
			path = di_devfs_minor_path(minor);
			sprintf(opath, "/devices%s",path);
			di_devfs_path_free(path);
			return open(opath, O_RDWR);
		}
		node = di_drv_next_node(node);
	}

	return (-1);
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

/* TODO: Use the same variable for datain and dataout */
static int
upci_rw(char *rwcom, int dev)
{
	char len;
	char datain[8], dataout[8];
	int i, reg, ioctlcmd, fd, rval = 0;
	uint64_t off;
	upci_rw_cmd_t rw;

	if (upci_parse_command(rwcom, &reg, &off, &len, datain) != 0) {
		fprintf(stderr, "Error: Failed to parse command\n");
		return (1);
	}

	rw.rw_region = reg;
	rw.rw_offset = off;
	rw.rw_count = len;
	rw.rw_pdatain = (uintptr_t) datain;
	rw.rw_pdataout = (uintptr_t) dataout;

	ioctlcmd = g_wflag ? UPCI_IOCTL_WRITE: UPCI_IOCTL_READ;

	if ((fd = upci_open_minor(dev)) == -1) {
		fprintf(stderr, "Error: Failed to open [%d]\n", dev);
		return (1);
	}

	if (ioctl(fd, ioctlcmd, &rw) != 0) {
		fprintf(stderr, "Error: I/O ioctl failed for [%d]\n", dev);
		rval = 1;
		goto out;
	}


	if (g_rflag) {
		for (i = 0; i < len; i++)
			printf("%02x", dataout[i] & 0xff);
		putchar('\n');
	}
out:
	close(fd);
	return (rval);
}

static int
upci_open_device(int dev)
{
	int fd, rval = 0;

	if ((fd = upci_open_minor(dev)) == -1) {
		fprintf(stderr, "Error: Failed to open [%d]\n", dev);
		return (1);
	}

	if (ioctl(fd, UPCI_IOCTL_OPEN, NULL) != 0) {
		fprintf(stderr, "Failed to open [%d]\n", dev);
		rval = 1;
	}
	close(fd);
	return (rval);
}

static int
upci_close_device(int dev)
{
	int fd, rval = 0;

	if ((fd = upci_open_minor(dev)) == -1) {
		fprintf(stderr, "Error: Failed to open [%d]\n", dev);
		return (1);
	}

	if (ioctl(fd, UPCI_IOCTL_CLOSE, NULL) != 0) {
		fprintf(stderr, "Failed to close [%d]\n", dev);
		rval = 1;
	}
	close(fd);
	return (rval);
}

static int
upci_open_device_regs(int dev)
{
	int fd, rval = 0;

	if ((fd = upci_open_minor(dev)) == -1) {
		fprintf(stderr, "Error: Failed to open [%d]\n", dev);
		return (1);
	}

	if (ioctl(fd, UPCI_IOCTL_OPEN_REGS, NULL) != 0) {
		fprintf(stderr, "Failed to open [%d]\n", dev);
		rval = 1;
	}
	close(fd);
	return (rval);
}

static int
upci_close_device_regs(int dev)
{
	int fd, rval = 0;

	if ((fd = upci_open_minor(dev)) == -1) {
		fprintf(stderr, "Error: Failed to open [%d]\n", dev);
		return (1);
	}

	if (ioctl(fd, UPCI_IOCTL_CLOSE_REGS, NULL) != 0) {
		fprintf(stderr, "Failed to close [%d]\n", dev);
		rval = 1;
	}
	close(fd);
	return (rval);
}

static int
upci_list_devices()
{
	int instance;
	di_minor_t minor;
	di_node_t node;
	char *path;
	dev_t dev;

	node = di_drv_first_node("upci", g_di_root);
	while (node != DI_NODE_NIL) {

		minor = DI_MINOR_NIL;
		instance = di_instance(node);
		if ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
			path = di_devfs_minor_path(minor);
			dev = di_minor_devt(minor);
			printf("[%02d %03d:%02d] /devices%s\n", instance,
			    major(dev), minor(dev), path);
			di_devfs_path_free(path);
		}
		node = di_drv_next_node(node);
	}

	return (0);
}

static int
upci_get_dev_info(int fd)
{
	int rval = 0;

	upci_dev_info_t di;
	if (ioctl(fd, UPCI_IOCTL_DEV_INFO, &di) != 0) {
		fprintf(stderr, "Failed to IOCTL [%d]\n");
		rval = 1;
		goto out;
	}

	printf("flags = %s %s\n",
	    di.di_flags & UPCI_DEVINFO_DEV_OPEN ? "UPCI_DEVINFO_DEV_OPEN" : "",
	    di.di_flags & UPCI_DEVINFO_REG_OPEN ? "UPCI_DEVINFO_REG_OPEN" : "");
	printf("nregs = %"PRIx64"\n", di.di_nregs);

out:
	close(fd);
	return (rval);
}

static int
upci_get_info(int reg, int dev)
{
	int fd, rval = 0;
	upci_reg_info_t ri;

	if ((fd = upci_open_minor(dev)) == -1) {
		fprintf(stderr, "Error: Failed to open [%d]\n", dev);
		return (1);
	}

	if (reg < 0)
		return upci_get_dev_info(fd);

	ri.ri_region = reg;
	if (ioctl(fd, UPCI_IOCTL_REG_INFO, &ri) != 0) {
		fprintf(stderr, "Failed to IOCTL [%d]\n", dev);
		rval = 1;
		goto out;
	}

	printf("region [%d]\n", reg);
	printf("  flags = %s %s %s\n",
	    ri.ri_flags & UPCI_IO_REG_IO ? "UPCI_IO_REG_IO" : "UPCI_IO_REG_MEM",
	    ri.ri_flags & UPCI_IO_REG_PREFETCH ? "UPCI_IO_REG_PREFETCH" : "",
	    ri.ri_flags & UPCI_IO_REG_VALID ? "UPCI_IO_REG_VALID" : "");
	printf("  base = x%"PRIx64"\n", ri.ri_base);
	printf("  size = x%"PRIx64"\n", ri.ri_size);
out:
	close(fd);
	return (rval);
}


int
main(int argc, char **argv)
{
	int c, dev, reg, rval = 1;
	char *devstr, *regstr, *rwcom, *str;

	regstr = devstr = rwcom = NULL;
	while((c = getopt(argc, argv, "lvd:ocOCi:r:w:h")) != EOF) {
		switch (c) {
		case 'l':
			g_lflag = B_TRUE;
		break;
		case 'v':
			g_vflag = B_TRUE;
		break;
		case 'd':
			g_dflag = B_TRUE;
			devstr = optarg;
		break;
		case 'o':
			g_oflag = B_TRUE;
		break;
		case 'c':
			g_cflag = B_TRUE;
		break;
		case 'O':
			g_Oflag = B_TRUE;
		break;
		case 'C':
			g_Cflag = B_TRUE;
		break;
		case 'i':
			g_iflag = B_TRUE;
			regstr = optarg;
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
			usage();
			goto out;
		break;
		}
	}

	if (g_lflag + g_oflag + g_cflag + g_Oflag +
	    g_Cflag + g_iflag + g_rflag + g_wflag != 1) {
		usage();
		goto out;
	}
	if (!g_lflag && !g_dflag) {
		usage();
		goto out;
	}

	if ((g_di_root = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
		fprintf(stderr, "Error: failed to initialize libdevinfo\n");
		goto out;
	}

	/* list upci devices */
	if (g_lflag) {
		rval = upci_list_devices();
		goto out2;
	}


	errno = 0;
	dev = strtol(devstr, &str, 10);
	if (*str || errno != 0) {
		usage();
		goto out2;
	}

	if (g_oflag) {
		rval = upci_open_device(dev);
	} else if (g_cflag) {
		rval = upci_close_device(dev);
	} else if (g_Oflag) {
		rval = upci_open_device_regs(dev);
	} else if (g_Cflag) {
		rval = upci_close_device_regs(dev);
	} else if (g_rflag || g_wflag) {
		rval = upci_rw(rwcom, dev);
	} else if (g_iflag) {
		errno = 0;
		reg = strtol(regstr, &str, 10);
		if (*str || errno != 0) {
			usage();
			goto out2;
		}
		rval = upci_get_info(reg, dev);
	}

out2:
	di_fini(g_di_root);
out:
	return (rval);
}
