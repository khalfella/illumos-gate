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

typedef struct upci_cmd_s {

	char		cm_devpath[MAXPATHLEN];	/* device physical path */
	uint64_t	cm_off;			/* offset for read/write */
	uint64_t	cm_uibuff;		/* userland buffer in */
	uint64_t	cm_uibufsz;		/* userland buffer in sz */
	uint64_t	cm_uobuff;		/* userland buffer out */
	uint64_t	cm_uobufsz;		/* userland buffer out sz */
} upci_cmd_t;

#define	UPCI_IOCTL_OPEN_DEVICE		0x01
#define	UPCI_IOCTL_CLOSE_DEVICE		0x02
#define	UPCI_IOCTL_CFG_READ		0x03
#define	UPCI_IOCTL_CFG_WRITE		0x04
#define	UPCI_IOCTL_REG_READ		0x05
#define	UPCI_IOCTL_REG_WRITE		0x06


/*
 * Add structures, function prototypes and other stuff here.
 */

typedef struct upci_devinfo_s {
	char		di_devpath[MAXPATHLEN];
	uint64_t	di_flags;
} upci_devinfo_t;

#define	UPCI_DEVINFO_CLOSED		0x00000001
#define	UPCI_DEVINFO_CFG_OPEN		0x00000002
#define	UPCI_DEVINFO_DEV_ENABLED	0x00000004
#define	UPCI_DEVINFO_REG_MAPPED		0x00000008
#define	UPCI_DEVINFO_INTX_ENABLED	0x00000010
#define	UPCI_DEVINFO_MSI_ENABLED	0x00000020
#define	UPCI_DEVINFO_MSIX_ENABLED	0x00000040

typedef struct upci_cfg_rw_s {
	char		rw_devpath[MAXPATHLEN];
	uint64_t	rw_offset;
	uint64_t	rw_count;
} upci_cfg_rw_t;


#define	UPCI_STRING			"Userland PCI Driver"

#ifdef __cplusplus
}
#endif

#endif /* _UPCI_H */

