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

	uint8_t		cm_seg;		/* pci device seg or domain */
	uint8_t		cm_bus;		/* pci bus */
	uint8_t		cm_dev;		/* pci device */
	uint8_t		cm_fun;		/* pci function */
	uint8_t		pad[4];		/* padding bytes */

	uint64_t	cm_off;		/* offset for read/write */
	uint64_t	cm_uibuff;	/* userland input buffer copyin() */
	uint64_t	cm_uibufsz;	/* userland input buffer size */
	uint64_t	cm_uobuff;	/* userland output buffer copyout() */
	uint64_t	cm_uobufsz;	/* userland output buffer size */
} upci_cmd_t;

#define	UPCI_IOCTL_CFG_READ	0x01
#define	UPCI_IOCTL_CFG_WRITE	0x02


/*
 * Add structures, function prototypes and other stuff here.
 */

#define UPCI_STRING "khalfella 123"

#ifdef __cplusplus
}
#endif

#endif /* _UPCI_H */

