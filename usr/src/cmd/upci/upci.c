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

#define UPCI_DEV	"/devices/pseudo/upci@0:upci"

int
main(void)
{
	int fd;
	upci_cmd_t cmd;
	char buff[100];

	if((fd = open("/devices/pseudo/upci@0:upci", O_RDONLY)) == -1) {
		fprintf(stderr, "Failed to open upci device\n");
		return (1);
	}

	memset(&cmd, 0, sizeof(upci_cmd_t));

	cmd.cm_uobuff =  (uint64_t) (buff - ((char*)NULL)); 

	printf("sizeof(upci_cmd_t) = %d\n", sizeof(upci_cmd_t));
	printf("buff = %p uobuff = %xll\n", buff, cmd.cm_uobuff);


	if (ioctl(fd, UPCI_IOCTL_CFG_READ, &cmd) != 0) {
		fprintf(stderr, "upci driver ioctl failed\n");
		return (1);
	}


	printf("ioctl output = \"%s\"\n", cmd.cm_uobuff);

        return (0);
}
