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
upci_xdma_alloc_coherent(dev_t dev, upci_coherent_t * uarg, cred_t *cr,
    int *rv)
{
	int rval;
	minor_t instance;
	upci_t *up;
	upci_coherent_t uch;
	upci_ch_ent_t *che;


	rval = *rv = EINVAL;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	if (!(up->up_flags & UPCI_DEVINFO_REG_OPEN) ||
	    copyin(uarg, &uch, sizeof(uch)) != 0) {
		rval = *rv = EIO;
		goto out;
	}

	che = kmem_alloc(sizeof(*che), KM_SLEEP);
	che->ch_length = uch.ch_length;

       if (ddi_dma_alloc_handle(up->up_dip, &upci_dma_attrs, DDI_DMA_SLEEP,
	    NULL, &che->ch_hdl) != DDI_SUCCESS) {
		rval = *rv = ENOMEM;
		goto free_ent;
       }

	if (ddi_dma_mem_alloc(che->ch_hdl, che->ch_length, &xdma_dev_attrs,
	    DDI_DMA_CONSISTENT | IOMEM_DATA_UNCACHED,
	    DDI_DMA_SLEEP, NULL, &che->ch_kaddr, &che->ch_real_length,
	    &che->ch_acc_hdl) != DDI_SUCCESS) {
		rval = *rv = ENOMEM;
		goto free_hdl;
	}

	if (ddi_dma_addr_bind_handle(che->ch_hdl, NULL, che->ch_kaddr,
	    che->ch_real_length, DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, NULL, &che->ch_cookie,
	    &che->ch_ncookies) != DDI_DMA_MAPPED) {
		rval = *rv = ENOMEM;
		goto free_mem;
	}

	if (che->ch_ncookies != 1) {
		rval = *rv = ENOMEM;
		goto unbind_mem;
	}

	uch.ch_cookie = che->ch_cookie.dmac_address;
	if (copyout(&uch, uarg, sizeof(uch)) == 0) {

		if (uch.ch_flags == 1) {
			bzero(che->ch_kaddr, che->ch_real_length);
		}
		list_insert_tail(&up->up_ch_xdma_list, che);
		mutex_exit(&up->up_lk);
		return (*rv = 0);
	}

unbind_mem:
	ddi_dma_unbind_handle(che->ch_hdl);
free_mem:
	ddi_dma_mem_free(&che->ch_acc_hdl);
free_hdl:
	ddi_dma_free_handle(&che->ch_hdl);
free_ent:
	kmem_free(che, sizeof(*che));
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

static upci_ch_ent_t *
find_xdma_coherent_map(upci_t *up, uint64_t cookie)
{
	upci_ch_ent_t *che;
	for (che = list_head(&up->up_ch_xdma_list); che != NULL;
	    che = list_next(&up->up_ch_xdma_list, che)) {
		if (cookie == che->ch_cookie.dmac_address) {
			return (che);
		}
	}

	return (NULL);
}

int
upci_xdma_free_coherent(dev_t dev, upci_coherent_t * uarg, cred_t *cr,
    int *rv)
{
	return (0);
}

int
upci_xdma_rw_coherent(dev_t dev, upci_coherent_t * uarg, cred_t *cr,
    int *rv, int write)
{
	int i, rval;
	minor_t instance;
	upci_t *up;
	upci_coherent_t uch;
	upci_ch_ent_t *che;
	char *src, *dst;


	rval = *rv = EINVAL;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	if (!(up->up_flags & UPCI_DEVINFO_REG_OPEN) ||
	    copyin(uarg, &uch, sizeof(uch)) != 0 ||
	    (che = find_xdma_coherent_map(up, uch.ch_cookie)) == NULL) {
		rval = *rv = EIO;
		goto out;
	}

	if (!write) {
		uch.ch_udata = 0;
		ddi_dma_sync(che->ch_hdl, 0, 0, DDI_DMA_SYNC_FORCPU);
	}

	*(write ? &src : &dst) = (char *) &uch.ch_udata;
	*(write ? &dst : &src) = (char *) (che->ch_kaddr + uch.ch_offset);

	if (write) {
		ddi_dma_sync(che->ch_hdl, 0, 0, DDI_DMA_SYNC_FORDEV);
	}

	for (i = 0; i < uch.ch_length; i++)
		dst[i] = src[i];

	if (copyout(&uch, uarg, sizeof(uch)) == 0) {
		rval = *rv = 0;
	}
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}
