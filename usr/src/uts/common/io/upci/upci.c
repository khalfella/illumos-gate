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

typedef struct upci_reg_s {
	caddr_t			reg_base;
	off_t			reg_size;
	ddi_acc_handle_t	reg_hdl;
} upci_reg_t;

typedef struct upci_s {
	dev_info_t		*up_dip;
	ddi_acc_handle_t 	up_hdl;
	kmutex_t		up_lk;
	uint_t			up_flags;
	int			up_nregs;
	upci_reg_t		*up_regs;
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
	return (0);
}

static int
upci_close_regs_ioctl(dev_t dev, cred_t *cr, int *rv)
{
	uint_t r;
	int rval = DDI_SUCCESS;
	minor_t instance;
	upci_t *up;

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
	for (r = 0; r < up->up_nregs; r++) {
		ddi_regs_map_free(&up->up_regs[r].reg_hdl);
	}

	up->up_flags &= ~UPCI_DEVINFO_REG_OPEN;
	kmem_free(up->up_regs, up->up_nregs * sizeof(upci_reg_t));
	up->up_regs = NULL;
	up->up_nregs = 0;
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

static int
upci_open_regs_ioctl(dev_t dev, cred_t *cr, int *rv)
{
	uint_t r;
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;
	dev_info_t *dip;
	upci_reg_t *regs;
	ddi_device_acc_attr_t reg_attr;

	reg_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	reg_attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	reg_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	*rv = 0;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);
	dip = up->up_dip;

	if (!(up->up_flags & UPCI_DEVINFO_DEV_OPEN) ||
	    (up->up_flags & UPCI_DEVINFO_REG_OPEN) ||
	    ddi_dev_nregs(dip,  &up->up_nregs) != DDI_SUCCESS) {
		rval = *rv = EIO;
		goto out2;
	}

	regs = up->up_regs = kmem_alloc(up->up_nregs * sizeof(upci_reg_t),
				KM_SLEEP);

	for (r = 0; r < up->up_nregs; r++) {
		if (ddi_dev_regsize(dip, r, &regs[r].reg_size) != DDI_SUCCESS ||
		    ddi_regs_map_setup(dip, r, &regs[r].reg_base, 0, 0,
		    &reg_attr, &regs[r].reg_hdl)!= DDI_SUCCESS) {
			rval = *rv = EIO;
			goto out2;
		}
	}

	up->up_flags |= UPCI_DEVINFO_REG_OPEN;

out2:
	mutex_exit(&up->up_lk);
	return (abs(rval));

}
static int
upci_open_device_ioctl(dev_t dev, cred_t *cr, int *rv)
{
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;

	*rv = 0;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	if (up->up_flags & UPCI_DEVINFO_DEV_OPEN) {
		rval = *rv = EINVAL;
		goto out2;
	}

	if (pci_config_setup(up->up_dip, &up->up_hdl) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "Error: filed to setup PCI config space [%d]\n",
		    instance);
		*rv = EIO;
		rval = DDI_FAILURE;
		goto out2;
	}
	up->up_flags |= UPCI_DEVINFO_DEV_OPEN;

out2:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

static int
upci_close_device_ioctl(dev_t dev, cred_t *cr, int *rv)
{
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;

	*rv = 0;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	if (!(up->up_flags & UPCI_DEVINFO_DEV_OPEN)) {
		rval = *rv = EINVAL;
		goto out;
	}
	/* Device config close code goes here */

	pci_config_teardown(&up->up_hdl);
	up->up_flags &= ~UPCI_DEVINFO_DEV_OPEN;
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

static int
upci_rw_reg(dev_t dev, upci_rw_cmd_t *rw, cred_t *cr, int *rv, int write)
{
	int rval = DDI_SUCCESS;
	uint64_t buff;
	upci_t *up;
	upci_reg_t *reg;
	minor_t instance;


	*rv = 0;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	if ((rw->rw_region >= up->up_nregs) ||
	    !(up->up_flags & UPCI_DEVINFO_REG_OPEN) ||
	    (rw->rw_count != 1 && rw->rw_count != 2 &&
	    rw->rw_count != 4 && rw->rw_count != 8)) {
		rval = *rv = EINVAL;
		goto out;
	}

	reg = &up->up_regs[rw->rw_region];

	if (write) goto write;
	/* Read */
	switch (rw->rw_count) {
	case 1:
		*((uint8_t *)&buff) = ddi_get8(
		    reg->reg_hdl, (uint8_t *)(reg->reg_base + rw->rw_offset));
	break;
	case 2:
		*((uint16_t *)&buff) = ddi_get16(
		    reg->reg_hdl, (uint16_t *)(reg->reg_base + rw->rw_offset));
	break;
	case 4:
		*((uint32_t *)&buff) = ddi_get32(
		    reg->reg_hdl, (uint32_t *)(reg->reg_base + rw->rw_offset));
	break;
	case 8:
		*((uint64_t *)&buff) = ddi_get64(
		    reg->reg_hdl, (uint64_t *)(reg->reg_base + rw->rw_offset));
	break;
	}
	if (copyout(&buff, (void *) rw->rw_pdataout, rw->rw_count) != 0) {
		rval = *rv = EINVAL;
	}

	goto out;

write:
	if (copyin((void *) rw->rw_pdatain, &buff, rw->rw_count) == 0) {
		switch (rw->rw_count) {
		case 1:
			ddi_put8(reg->reg_hdl,
			    (uint8_t *)(reg->reg_base + rw->rw_offset),
			    *((uint8_t *)&buff));
		break;
		case 2:
			ddi_put16(reg->reg_hdl,
			    (uint16_t *)(reg->reg_base + rw->rw_offset),
			    *((uint16_t *)&buff));
		break;
		case 4:
			ddi_put32(reg->reg_hdl,
			    (uint32_t *)(reg->reg_base + rw->rw_offset),
			    *((uint32_t *)&buff));
		break;
		case 8:
			ddi_put64(reg->reg_hdl,
			    (uint64_t *)(reg->reg_base + rw->rw_offset),
			    *((uint64_t *)&buff));
		break;
		}
		goto out;
	}

	/* Handle error */
	rval = *rv = EINVAL;
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

static int
upci_rw_ioctl(dev_t dev, upci_rw_cmd_t *rw, cred_t *cr, int *rv, int write)
{
	int rval = DDI_SUCCESS;
	uint64_t buff;
	upci_t *up;
	minor_t instance;

	*rv = 0;
	instance = getminor(dev);

	if (rw->rw_region != -1)
		return upci_rw_reg(dev, rw, cr, rv, write);

	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);

	/* TODO: Check offset + count <= PCI config space size */
	if (!(up->up_flags & UPCI_DEVINFO_DEV_OPEN) ||
	    (rw->rw_count != 1 && rw->rw_count != 2 &&
	    rw->rw_count != 4 && rw->rw_count != 8)) {
		rval = *rv = EINVAL;
		goto out;
	}
	if (write) goto write;

	/* Read */
	switch (rw->rw_count) {
	case 1:
		*((uint8_t *)&buff) = pci_config_get8(up->up_hdl,
		    rw->rw_offset);
	break;
	case 2:
		*((uint16_t *)&buff) = pci_config_get16(up->up_hdl,
		    rw->rw_offset);
	break;
	case 4:
		*((uint32_t *)&buff) = pci_config_get32(up->up_hdl,
		    rw->rw_offset);
	break;
	case 8:
		*((uint64_t *)&buff) = pci_config_get64(up->up_hdl,
		    rw->rw_offset);
	break;
	}

	if (copyout(&buff, (void *) rw->rw_pdataout, rw->rw_count) != 0) {
		rval = *rv = EINVAL;
	}

	goto out;
write:
	if (copyin((void *) rw->rw_pdatain, &buff, rw->rw_count) == 0) {
		switch (rw->rw_count) {
		case 1:
			pci_config_put8(up->up_hdl, rw->rw_offset,
			    *((uint8_t *)&buff));
		break;
		case 2:
			pci_config_put16(up->up_hdl, rw->rw_offset,
			    *((uint16_t *)&buff));
		break;
		case 4:
			pci_config_put32(up->up_hdl, rw->rw_offset,
			    *((uint32_t *)&buff));
		break;
		case 8:
			pci_config_put64(up->up_hdl, rw->rw_offset,
			    *((uint64_t *)&buff));
		break;
		}
		goto out;
	}

	/* Handle error */
	rval = *rv = EINVAL;
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}


static int
upci_ioctl(dev_t dev, int cmd, intptr_t arg, int md, cred_t *cr, int *rv)
{
	int rval;
	upci_rw_cmd_t *rw;

	
	switch (cmd) {
	case UPCI_IOCTL_OPEN:
		rval = upci_open_device_ioctl(dev, cr, rv);
	break;
	case UPCI_IOCTL_CLOSE:
		rval = upci_close_device_ioctl(dev, cr, rv);
	break;
	case UPCI_IOCTL_OPEN_REGS:
		rval = upci_open_regs_ioctl(dev, cr, rv);
	break;
	case UPCI_IOCTL_CLOSE_REGS:
		rval = upci_close_regs_ioctl(dev, cr, rv);
	break;
	case UPCI_IOCTL_READ:
	case UPCI_IOCTL_WRITE:
		rval = *rv = EINVAL;
		rw = kmem_alloc(sizeof(*rw), KM_SLEEP);
		if (copyin((void *)arg, rw, sizeof(*rw)) == 0) {
			rval = upci_rw_ioctl(dev, rw,cr, rv,
			    cmd == UPCI_IOCTL_WRITE ? 1 : 0);
		}
		kmem_free(rw, sizeof(*rw));
	break;
	default:
		rval = *rv = EINVAL;
	break;
	}

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
	up->up_flags = 0;
	up->up_nregs = 0;
	up->up_regs = NULL;

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

	if (up == NULL || up->up_flags != 0)
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
