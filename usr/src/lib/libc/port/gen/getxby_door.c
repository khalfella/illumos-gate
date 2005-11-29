/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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

/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include "synonyms.h"
#include <mtlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <nss_dbdefs.h>
#include <stdio.h>
#include <synch.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <getxby_door.h>
#include <sys/door.h>
#include "libc.h"
#include "base_conversion.h"

/*
 *
 * Routine that actually performs the door call.
 * Note that we cache a file descriptor.  We do
 * the following to prevent disasters:
 *
 * 1) Never use 0,1 or 2; if we get this from the open
 *    we dup it upwards.
 *
 * 2) Set the close on exec flags so descriptor remains available
 *    to child processes.
 *
 * 3) Verify that the door is still the same one we had before
 *    by using door_info on the client side.
 *
 *	Note that we never close the file descriptor if it isn't one
 *	we allocated; we check this with door info.  The rather tricky
 *	logic is designed to be fast in the normal case (fd is already
 *	allocated and is ok) while handling the case where the application
 *	closed it underneath us or where the nscd dies or re-execs itself
 *	and we're a multi-threaded application.  Note that we cannot protect
 *	the application if it closes the fd and it is multi-threaded.
 *
 *  int _nsc_trydoorcall(void *dptr, int *bufsize, int *actualsize);
 *
 *      *dptr           IN: points to arg buffer OUT: points to results buffer
 *      *bufsize        IN: overall size of buffer OUT: overall size of buffer
 *      *actualsize     IN: size of call data OUT: size of return data
 *
 *  Note that *dptr may change if provided space as defined by *bufsize is
 *  inadequate.  In this case the door call mmaps more space and places
 *  the answer there and sets dptr to contain a pointer to the space, which
 *  should be freed with munmap.
 *
 *  Returns 0 if the door call reached the server, -1 if contact was not made.
 *
 */

static mutex_t	_door_lock = DEFAULTMUTEX;

int
_nsc_trydoorcall(nsc_data_t **dptr, int *ndata, int *adata)
{
	static	int 		doorfd = -1;
	static	door_info_t 	real_door;
	door_info_t 		my_door;
	door_arg_t		param;

	/*
	 * the first time in we try and open and validate the door.
	 * the validations are that the door must have been
	 * created with the name service door cookie and
	 * that the file attached to the door is owned by root
	 * and readonly by user, group and other.  If any of these
	 * validations fail we refuse to use the door.
	 */

	lmutex_lock(&_door_lock);

try_again:

	if (doorfd == -1) {
		int		tbc[3];
		int		i;

		if ((doorfd = open64(NAME_SERVICE_DOOR, O_RDONLY, 0)) == -1) {
			lmutex_unlock(&_door_lock);
			return (NOSERVER);
		}

		/*
		 * dup up the file descriptor if we have 0 - 2
		 * to avoid problems with shells stdin/out/err
		 */
		i = 0;

		while (doorfd < 3) { /* we have a reserved fd */
			tbc[i++] = doorfd;
			if ((doorfd = dup(doorfd)) < 0) {
				while (i--)
				    (void) close(tbc[i]);
				doorfd = -1;
				lmutex_unlock(&_door_lock);
				return (NOSERVER);
			}
		}

		while (i--)
		    (void) close(tbc[i]);

		/*
		 * mark this door descriptor as close on exec
		 */
		(void) fcntl(doorfd, F_SETFD, FD_CLOEXEC);
		if (__door_info(doorfd, &real_door) == -1 ||
		    (real_door.di_attributes & DOOR_REVOKED) ||
		    real_door.di_data != (uintptr_t)NAME_SERVICE_DOOR_COOKIE) {
			/*
			 * we should close doorfd because we just opened it
			 */
			(void) close(doorfd);
			doorfd = -1;
			lmutex_unlock(&_door_lock);
			return (NOSERVER);
		}
	} else {
		if (__door_info(doorfd, &my_door) == -1 ||
		    my_door.di_data != (uintptr_t)NAME_SERVICE_DOOR_COOKIE ||
		    my_door.di_uniquifier != real_door.di_uniquifier) {
			/*
			 * don't close it -
			 * someone else has clobbered fd
			 */
			doorfd = -1;
			goto try_again;
		}

		if (my_door.di_attributes & DOOR_REVOKED) {
			(void) close(doorfd);	/* nscd exited .... */
			doorfd = -1;	/* try and restart connection */
			goto try_again;
		}
	}

	lmutex_unlock(&_door_lock);

	param.rbuf = (char *)*dptr;
	param.rsize = *ndata;
	param.data_ptr = (char *)*dptr;
	param.data_size = *adata;
	param.desc_ptr = NULL;
	param.desc_num = 0;
	if (__door_call(doorfd, &param) == -1) {
		return (NOSERVER);
	}
	*adata = (int)param.data_size;
	*ndata = (int)param.rsize;
	*dptr = (nsc_data_t *)(uintptr_t)param.data_ptr;
	if (*adata == 0 || *dptr == NULL) {
		return (NOSERVER);
	}

	return ((*dptr)->nsc_ret.nsc_return_code);
}
