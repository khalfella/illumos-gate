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

#define	abs(a) ((a) < 0 ? -(a) : (a))

typedef struct upci_s {
	dev_info_t		*up_dip;
	ddi_acc_handle_t 	up_hdl;
	kmutex_t		up_lk;
	uint64_t		up_flags;
} upci_t;

static void		*soft_state_p;

static int
upci_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
    return (0);
}

static int
upci_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
    return (0);
}

static int
upci_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	/*
	 * A real driver should be more efficient, we always supply a single
	 * byte.
	 */
	const unsigned char data = 42;

	return (uiomove((void *)&data, sizeof (data), UIO_READ, uiop));
}


static int
upci_list_open_device_ioctl(dev_t dev, upci_cmd_t *ucmd, upci_cmd_t *ucmdup,
    cred_t *cr, int *rv)
{
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;
	dev_info_t *dip;
	upci_devinfo_t *pdi;

	*rv = 0;
	instance = getminor(dev);
	pdi = kmem_alloc(sizeof (*pdi), KM_SLEEP);
	if (ucmd->cm_uibufsz >= MAXPATHLEN ||
	    copyin((void *) ucmd->cm_uibuff, pdi->di_devpath,
	    ucmd->cm_uibufsz) != 0 ||
	    pdi->di_devpath[ucmd->cm_uibufsz - 1] != '\0' ||
	    (up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		goto out;
	}

	mutex_enter(&up->up_lk);

	if (up->up_flags != UPCI_DEVINFO_CLOSED) {
		rval = *rv = EINVAL;
		goto out2;
	}

	/* Device config open code goes here */

	dip = up->up_dip;

	if (pci_config_setup(dip, &up->up_hdl) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "Error: filed to setup PCI config space [%d]\n",
		    instance);
		*rv = EIO;
		rval = DDI_FAILURE;
	}
	up->up_flags = UPCI_DEVINFO_CFG_OPEN;

out2:
	mutex_exit(&up->up_lk);
out:
	kmem_free(pdi, sizeof (*pdi));
	return (abs(rval));
}

static int
upci_list_close_device_ioctl(dev_t dev, upci_cmd_t *ucmd, upci_cmd_t *ucmdup, cred_t *cr,
    int *rv)
{
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;
	upci_devinfo_t *pdi;

	*rv = 0;
	instance = getminor(dev);
	pdi = kmem_alloc(sizeof (*pdi), KM_SLEEP);
	if (ucmd->cm_uibufsz >= MAXPATHLEN ||
	    copyin((void *) ucmd->cm_uibuff, pdi->di_devpath,
	    ucmd->cm_uibufsz) != 0 ||
	    pdi->di_devpath[ucmd->cm_uibufsz - 1] != '\0' ||
	    (up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		goto out;
	}

	mutex_enter(&up->up_lk);

	if (up->up_flags != UPCI_DEVINFO_CFG_OPEN) {
		rval = *rv = EINVAL;
		goto out2;
	}
	/* Device config close code goes here */

	pci_config_teardown(&up->up_hdl);
	up->up_flags = UPCI_DEVINFO_CLOSED;

out2:
	mutex_exit(&up->up_lk);
out:
	kmem_free(pdi, sizeof (*pdi));
	return (abs(rval));
}


static int
upci_cfg_rw_ioctl(dev_t dev, upci_cmd_t *ucmd, upci_cmd_t *ucmdup, cred_t *cr, int *rv,
    int write)
{
	int rval = DDI_SUCCESS;
	uint64_t buff;
	upci_t *up;
	minor_t instance;
	upci_cfg_rw_t *crw;

	*rv = 0;
	instance = getminor(dev);
	crw = kmem_alloc(sizeof (*crw), KM_SLEEP);
	if (ucmd->cm_uibufsz != sizeof (upci_cfg_rw_t) ||
	    copyin((void *) ucmd->cm_uibuff, crw, ucmd->cm_uibufsz) != 0 ||
	    (up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		goto out;
	}

	mutex_enter(&up->up_lk);

	/* TODO: Check offset + count <= PCI config space size */
	if (!(up->up_flags & UPCI_DEVINFO_CFG_OPEN) ||
	    (crw->rw_count != 1 && crw->rw_count != 2 &&
	    crw->rw_count != 4 && crw->rw_count != 8) ||
	    ucmd->cm_uobufsz != crw->rw_count) {
		rval = *rv = EINVAL;
		goto out2;
	}
	if (write) goto write;

	/* Read */
	switch (crw->rw_count) {
	case 1:
		*((uint8_t *)&buff) = pci_config_get8(up->up_hdl,
		    crw->rw_offset);
	break;
	case 2:
		*((uint16_t *)&buff) = pci_config_get16(up->up_hdl,
		    crw->rw_offset);
	break;
	case 4:
		*((uint32_t *)&buff) = pci_config_get32(up->up_hdl,
		    crw->rw_offset);
	break;
	case 8:
		*((uint64_t *)&buff) = pci_config_get64(up->up_hdl,
		    crw->rw_offset);
	break;
	}

	if (copyout(&buff, (void *) ucmd->cm_uobuff, crw->rw_count) == 0)
		goto out2;
write:
	if (copyin((void *) ucmd->cm_uobuff, &buff, crw->rw_count) == 0) {
		switch (crw->rw_count) {
		case 1:
			pci_config_put8(up->up_hdl, crw->rw_offset,
			    *((uint8_t *)&buff));
		break;
		case 2:
			pci_config_put16(up->up_hdl, crw->rw_offset,
			    *((uint16_t *)&buff));
		break;
		case 4:
			pci_config_put32(up->up_hdl, crw->rw_offset,
			    *((uint32_t *)&buff));
		break;
		case 8:
			pci_config_put64(up->up_hdl, crw->rw_offset,
			    *((uint64_t *)&buff));
		break;
		}
		goto out2;
	}

	/* Handle error */
	rval = *rv = EINVAL;
out2:
	mutex_exit(&up->up_lk);
out:
	kmem_free(crw, sizeof (*crw));
	return (abs(rval));
}


static int
upci_ioctl(dev_t dev, int cmd, intptr_t arg, int md, cred_t *cr, int *rv)
{
	int rval = DDI_SUCCESS;
	upci_cmd_t *ucmd  = kmem_alloc(sizeof(*ucmd), KM_SLEEP);
	if (copyin((void *)arg, ucmd, sizeof(*ucmd)) != 0) {
		rval = *rv = EINVAL;
		goto out;
	}

	
	switch (cmd) {
	case UPCI_IOCTL_OPEN_DEVICE:
		rval = upci_list_open_device_ioctl(dev, ucmd, (upci_cmd_t *)arg,
		    cr, rv);
	break;
	case UPCI_IOCTL_CLOSE_DEVICE:
		rval = upci_list_close_device_ioctl(dev, ucmd, (upci_cmd_t *)arg,
		    cr, rv);
	break;
	case UPCI_IOCTL_CFG_READ:
		rval = upci_cfg_rw_ioctl(dev, ucmd, (upci_cmd_t *)arg,
		    cr, rv, 0);
	break;
	case UPCI_IOCTL_CFG_WRITE:
		rval = upci_cfg_rw_ioctl(dev, ucmd, (upci_cmd_t *)arg,
		    cr, rv, 1);
	break;
	default:
		*rv = EINVAL;
	break;
	}

out:
	kmem_free(ucmd, sizeof(*ucmd));
	return (abs(rval));
}

static int
upci_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
	int ret = DDI_FAILURE;

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		/*
		 *resultp = upci_dip;
		 */
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*resultp = (void *)((uintptr_t)getminor((dev_t)arg));
		ret = DDI_SUCCESS;
		break;
	default:
		break;
	}

	return (ret);
}

static int
upci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	upci_t *up;
	minor_t instance;

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	instance = ddi_get_instance(dip);

	if (ddi_create_minor_node(dip, "upci", S_IFCHR, instance,
	    DDI_PSEUDO,  0) == DDI_FAILURE)
		return (DDI_FAILURE);

	if (ddi_soft_state_zalloc(soft_state_p, instance) != DDI_SUCCESS) {
		ddi_remove_minor_node(dip, "upci");
		return (DDI_FAILURE);
	}
	up = ddi_get_soft_state(soft_state_p, instance);

	/* Initialize device properties */
	mutex_init(&up->up_lk, NULL, MUTEX_DRIVER, NULL);
	up->up_dip = dip;
	up->up_flags = UPCI_DEVINFO_CLOSED;

	ddi_set_driver_private(dip, (caddr_t)up);

	cmn_err(CE_CONT, "Attached [%d]\n", instance);

	return (DDI_SUCCESS);
}

static int
upci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	upci_t *up;
	minor_t instance;

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	instance = ddi_get_instance(dip);
	up = ddi_get_driver_private(dip);

	if (up == NULL || up->up_flags != UPCI_DEVINFO_CLOSED)
		return (DDI_FAILURE);

	cmn_err(CE_CONT, "Detached [%d]\n", instance);

	ddi_remove_minor_node(dip, "upci");
	mutex_destroy(&up->up_lk);
	ddi_soft_state_free(soft_state_p, instance);
	return (DDI_SUCCESS);
}

static struct cb_ops upci_cb_ops = {
	upci_open,	/* open */
	upci_close,	/* close */
	nulldev,	/* strategy */
	nulldev,	/* print */
	nodev,		/* dump */
	upci_read,	/* read */
	nodev,		/* write */
	upci_ioctl,	/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,	/* cb_prop_op */
	NULL,		/* streamtab  */
	D_MP		/* Driver compatibility flag */
};

static struct dev_ops upci_dev_ops = {
	DEVO_REV,			/* devo_rev */
	0,				/* refcnt */
	upci_getinfo,			/* get_dev_info */
	nulldev,			/* identify */
	nulldev,			/* probe */
	upci_attach,			/* attach */
	upci_detach,			/* detach */
	nodev,				/* reset */
	&upci_cb_ops,			/* driver operations */
	NULL,				/* bus operations */
	nodev,				/* dev power */
	ddi_quiesce_not_needed		/* quiesce */
};

static struct modldrv upci_modldrv = {
	&mod_driverops,
	"user pci access",
	&upci_dev_ops
};

static struct modlinkage upci_modlinkage = {
	MODREV_1,
	&upci_modldrv,
	NULL
};

int
_init(void)
{
	if (ddi_soft_state_init(&soft_state_p, sizeof(upci_t), 0) != 0)
		return DDI_FAILURE;

	cmn_err(CE_CONT, "Initialized upci\n");

	return (mod_install(&upci_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&upci_modlinkage, modinfop));
}

int
_fini(void)
{
	ddi_soft_state_fini(&soft_state_p);
	return (mod_remove(&upci_modlinkage));
}
