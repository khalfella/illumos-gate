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

#ifndef _UPCI_H
#define _UPCI_H

/*
 * add other includes here
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	UPCI_IOCTL_OPEN		0x01
#define	UPCI_IOCTL_CLOSE	0x02
#define	UPCI_IOCTL_READ		0x03
#define	UPCI_IOCTL_WRITE	0x04
#define	UPCI_IOCTL_GET_INFO	0x04


/*
 * Add structures, function prototypes and other stuff here.
 */

/*
 * UPCI_IOCTL_GET_INFO
 */
#define	UPCI_DEVINFO_DEV_OPEN	0x01

typedef struct upci_devinfo_s {
	uint64_t	di_flags;
} upci_devinfo_t;

/*
 * UPCI_IOCTL_READ
 * UPCI_IOCTL_WRITE
 */
typedef struct upci_rw_cmd_s {
	int64_t		rw_region;
	uint64_t	rw_offset;
	uint64_t	rw_count;
	uint64_t	rw_pdatain;
	uint64_t	rw_pdataout;
} upci_rw_cmd_t;

#ifdef __cplusplus
}
#endif

#endif /* _UPCI_H */

