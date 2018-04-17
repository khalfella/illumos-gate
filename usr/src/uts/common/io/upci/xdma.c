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
 * upci: user pci driver
 */

#include <sys/stddef.h>
#include <sys/cmn_err.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/types.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/ksynch.h>
#include <sys/pci_impl.h>
#include <sys/ddi_isa.h>
#include <sys/upci.h>

#include "upci_sw.h"

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define	abs(a) ((a) < 0 ? -(a) : (a))

extern void *soft_state_p;

static ddi_dma_attr_t upci_dma_attrs = {
	DMA_ATTR_V0,		/* Version number */
	0x00000000,		/* low address */
	0xFFFFFFFF,		/* high address */
	0xFFFFFFFF,		/* counter register max */
	4096,			/* page alignment */
	0x7ff,			/* burst sizes: (any) */
	0x1,			/* minimum transfer size */
	0xFFFFFFFF,		/* max transfer size */
	0xFFFFFFFF,		/* address register max */
	1,			/* no scatter-gather */
	1,			/* byte granularity */
	DDI_DMA_FLAGERR,	/* attr flag */
};


static ddi_device_acc_attr_t xdma_dev_attrs = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_LE_ACC,
	DDI_STRICTORDER_ACC
};

int
upci_xdma_alloc(dev_t dev, upci_dma_t * uarg, cred_t *cr, int *rv)
{
	int rval;
	minor_t instance;
	upci_t *up;
	upci_dma_t udma;
	upci_xdma_ent_t *xde;


	rval = *rv = EINVAL;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	if (!(up->up_flags & UPCI_DEVINFO_REG_OPEN) ||
	    copyin(uarg, &udma, sizeof(udma)) != 0) {
		rval = *rv = EIO;
		goto out;
	}

	xde = kmem_alloc(sizeof(*xde), KM_SLEEP);
	if (ddi_dma_alloc_handle(up->up_dip, &upci_dma_attrs, DDI_DMA_SLEEP,
	    NULL, &xde->xe_hdl) != DDI_SUCCESS) {
		rval = *rv = ENOMEM;
		goto free_ent;
	}

	xde->xe_flags = IOMEM_DATA_UNCACHED;
	xde->xe_flags |= udma.ud_type == DDI_DMA_CONSISTENT ?
	    DDI_DMA_CONSISTENT : DDI_DMA_STREAMING;
	xde->xe_length = udma.ud_length;
	if (ddi_dma_mem_alloc(xde->xe_hdl, xde->xe_length, &xdma_dev_attrs,
	    xde->xe_flags, DDI_DMA_SLEEP, NULL, &xde->xe_kaddr,
	    &xde->xe_real_length, &xde->xe_acc_hdl) != DDI_SUCCESS) {
		rval = *rv = ENOMEM;
		goto free_hdl;
	}

	if (ddi_dma_addr_bind_handle(xde->xe_hdl, NULL, xde->xe_kaddr,
	    xde->xe_real_length, DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, NULL, &xde->xe_cookie,
	    &xde->xe_ncookies) != DDI_DMA_MAPPED) {
		rval = *rv = ENOMEM;
		goto free_mem;
	}

	if (xde->xe_ncookies != 1) {
		rval = *rv = ENOMEM;
		goto unbind_mem;
	}

	udma.ud_host_phys = xde->xe_cookie.dmac_address;
	if (copyout(&udma, uarg, sizeof(udma)) == 0) {

		if (udma.ud_write == 1) {
			bzero(xde->xe_kaddr, xde->xe_real_length);
		}
		list_insert_tail(&up->up_xdma_list, xde);
		mutex_exit(&up->up_lk);
		return (*rv = 0);
	}

unbind_mem:
	ddi_dma_unbind_handle(xde->xe_hdl);
free_mem:
	ddi_dma_mem_free(&xde->xe_acc_hdl);
free_hdl:
	ddi_dma_free_handle(&xde->xe_hdl);
free_ent:
	kmem_free(xde, sizeof(*xde));
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

static upci_xdma_ent_t *
find_xdma_map(upci_t *up, uint64_t cookie)
{
	upci_xdma_ent_t *xde;
	for (xde = list_head(&up->up_xdma_list); xde != NULL;
	    xde = list_next(&up->up_xdma_list, xde)) {
		if (cookie == xde->xe_cookie.dmac_address) {
			return (xde);
		}
	}

	return (NULL);
}

int
upci_xdma_remove(dev_t dev, upci_dma_t * uarg, cred_t *cr, int *rv)
{
	int rval;
	minor_t instance;
	upci_t *up;
	upci_dma_t udma;
	upci_xdma_ent_t *xde;

	rval = *rv = 0;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		return (*rv = EINVAL);
	}

	mutex_enter(&up->up_lk);

	if (!(up->up_flags & UPCI_DEVINFO_REG_OPEN) ||
	    copyin(uarg, &udma, sizeof(udma)) != 0 ||
	    (xde = find_xdma_map(up, udma.ud_host_phys)) == NULL ||
	    ddi_dma_unbind_handle(xde->xe_hdl) != DDI_SUCCESS) {
		rval = *rv = EIO;
		goto out;
	}

	ddi_dma_mem_free(&xde->xe_acc_hdl);
	ddi_dma_free_handle(&xde->xe_hdl);
	list_remove(&up->up_xdma_list, xde);
	kmem_free(xde, sizeof(*xde));
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

int
upci_xdma_rw(dev_t dev, upci_dma_t * uarg, cred_t *cr, int *rv)
{
	int i, rval;
	minor_t instance;
	upci_t *up;
	upci_dma_t udma;
	upci_xdma_ent_t *xde;
	char *src, *dst;

	rval = *rv = EINVAL;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	if (!(up->up_flags & UPCI_DEVINFO_REG_OPEN) ||
	    copyin(uarg, &udma, sizeof(udma)) != 0 ||
	    (xde = find_xdma_map(up, udma.ud_host_phys)) == NULL) {
		rval = *rv = EIO;
		goto out;
	}


	if (!udma.ud_write) {
		udma.ud_udata = 0;
		if (xde->xe_flags & DDI_DMA_CONSISTENT) {
			ddi_dma_sync(xde->xe_hdl, 0, 0, DDI_DMA_SYNC_FORCPU);
		}
	}

	*(udma.ud_write ? &src : &dst) = (char *) &udma.ud_udata;
	*(udma.ud_write ? &dst : &src) = (char *) (xde->xe_kaddr + udma.ud_rwoff);

	for (i = 0; i < udma.ud_length; i++)
		dst[i] = src[i];

	if (udma.ud_write && (xde->xe_flags & DDI_DMA_CONSISTENT)) {
		ddi_dma_sync(xde->xe_hdl, 0, 0, DDI_DMA_SYNC_FORDEV);
	}

	if (copyout(&udma, uarg, sizeof(udma)) == 0) {
		rval = *rv = 0;
	}
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}
