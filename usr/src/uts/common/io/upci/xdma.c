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

static ddi_dma_attr_t dma_attrs = {
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

static ddi_device_acc_attr_t dev_attrs = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_LE_ACC,
	DDI_STRICTORDER_ACC
};

int
upci_xdma_alloc_coherent(dev_t dev, upci_coherent_t * uarg, cred_t *cr,
    int *rv)
{
	uint_t r;
	int rval = DDI_SUCCESS;
	minor_t instance;
	upci_t *up;
	ddi_dma_handle_t dma_hdl;


	*rv = 0;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	if (!(up->up_flags & UPCI_DEVINFO_REG_OPEN)) {
		rval = *rv = EIO;
		goto out;
	}

	 if (ddi_dma_alloc_handle(up->up_dip, dma_attrs, DDI_DMA_SLEEP, NULL,
		&dma_hdl) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "Error: filed to allocate dma_handle [%d]\n",
		rval = *rv = EIO;
		goto out;
	}


	if (ddi_dma_mem_alloc(dma_hdl, size, &dev_attrs,
	    DDI_DMA_CONSISTENT | IOMEM_DATA_UNCACHED,
	    DDI_DMA_SLEEP, NULL, &dma->buf, &dma->bufLen,
	    &dma->dataHandle) != DDI_SUCCESS) {
		VMXNET3_WARN(dp, "ddi_dma_mem_alloc() failed");
		err = ENOMEM;
		goto error_dma_handle;
	}

out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

int
upci_xdma_free_coherent(dev_t dev, upci_coherent_t * uarg, cred_t *cr,
    int *rv)
{
	return (0);
}

int
upci_xdma_read_coherent(dev_t dev, upci_coherent_t * uarg, cred_t *cr,
    int *rv)
{
	return (0);
}

int
upci_xdma_write_coherent(dev_t dev, upci_coherent_t * uarg, cred_t *cr,
    int *rv)
{
	return (0);
}
