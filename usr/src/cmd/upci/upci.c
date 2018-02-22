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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/stat.h>


#include <sys/upci.h>

#define UPCI_DEV	"/devices/pci@0,0/pci15ad,1976@0:upci"

int
main(void)
{
	int i, fd;
	upci_cmd_t cmd;
	char buff[100];

	if((fd = open(UPCI_DEV, O_RDONLY)) == -1) {
		fprintf(stderr, "Failed to open upci device\n");
		return (1);
	}

	memset(&cmd, 0, sizeof(upci_cmd_t));

	cmd.cm_uobuff =  (uint64_t) (buff - ((char*)NULL)); 

	printf("sizeof(upci_cmd_t) = %d\n", sizeof(upci_cmd_t));
	printf("buff = %p uobuff = %xll\n", buff, cmd.cm_uobuff);


	if (ioctl(fd, UPCI_IOCTL_CFG_READ, &cmd) != 0) {
		fprintf(stderr, "upci driver CFG_READ ioctl failed\n");
		return (1);
	}

	printf("CFG_READ ioctl output = \"%s\"\n", cmd.cm_uobuff);

	upci_devinfo_t devs[3];
	cmd.cm_uobufsz = sizeof (devs);
	cmd.cm_uobuff = (uint64_t) ((intptr_t) devs);

	printf("cmd.cm_uobufsz = %xll cmd.cm_uobuff = %xll\n",
	    cmd.cm_uobufsz, cmd.cm_uobuff);

	if (ioctl(fd, UPCI_IOCTL_LIST_DEVICES, &cmd) != 0) {
		fprintf(stderr, "upci driver LIST_DEVICES ioctl failed\n");
		return (1);
	}

	for (i = 0; i < cmd.cm_uobufsz / sizeof (upci_devinfo_t); i++) {
		printf("dev[%d]: di_devpath = \"%s\" flags = %x\n",
		    i, devs[i].di_devpath, devs[i].di_flags);
	}



        return (0);
}
