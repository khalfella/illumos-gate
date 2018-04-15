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

#define	abs(a) ((a) < 0 ? -(a) : (a))

void *soft_state_p;

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
		if (up->up_regs[r].reg_flags & UPCI_IO_REG_VALID)
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
upci_get_reg_info_ioctl(dev_t dev, upci_reg_info_t *uri, cred_t *cr, int *rv)
{
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;
	upci_reg_t *regs;
	upci_reg_info_t ri;

	*rv = 0;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}
	mutex_enter(&up->up_lk);

	regs = up->up_regs;

	if (!(up->up_flags && UPCI_DEVINFO_REG_OPEN) ||
	    copyin(uri, &ri, sizeof(ri)) != 0 ||
	    ri.ri_region < 0 || ri.ri_region >= up->up_nregs) {
		rval = *rv = EINVAL;
		goto out;
	}

	ri.ri_flags = regs[ri.ri_region].reg_flags;
	ri.ri_base = (uint64_t) regs[ri.ri_region].reg_base;
	ri.ri_size = regs[ri.ri_region].reg_size;

	if (copyout(&ri, uri, sizeof (ri)) != 0) {
		rval = *rv = EINVAL;
		goto out;
	}
out:
	mutex_exit(&up->up_lk);
	return (abs(rval));
}

static int
upci_open_regs_ioctl(dev_t dev, cred_t *cr, int *rv)
{
	uint_t r;
	uint8_t rflags;
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
		regs[r].reg_flags = UPCI_IO_REG_VALID;
		if (ddi_dev_regsize(dip, r + 1, &regs[r].reg_size) != DDI_SUCCESS ||
		    ddi_regs_map_setup(dip, r + 1, &regs[r].reg_base, 0, 0,
		    &reg_attr, &regs[r].reg_hdl)!= DDI_SUCCESS) {
			regs[r].reg_flags &= ~UPCI_IO_REG_VALID;
			continue;
		}

		rflags = pci_config_get8(up->up_hdl, 0x10 + 0x4 * r);
		regs[r].reg_flags |= (rflags & UPCI_IO_REG_IO);
		if (!(rflags & UPCI_IO_REG_IO)) {
			regs[r].reg_flags |= (rflags & UPCI_IO_REG_PREFETCH);
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
upci_devinfo_ioctl(dev_t dev, upci_dev_info_t *udi, cred_t *cr, int *rv)
{
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;
	upci_dev_info_t di;

	*rv = 0;
	instance = getminor(dev);

	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_lk);
	di.di_flags = up->up_flags;
	di.di_nregs = up->up_nregs;
	mutex_exit(&up->up_lk);


	if (copyout(&di, udi, sizeof(di)) != 0) {
		rval = *rv = EINVAL;
	}
	return (abs(rval));
}

static uint_t
upci_intx_handler(caddr_t arg1, caddr_t arg2)
{
	upci_t *up;
	upci_intx_int_ent_t *ie;

	if ((ie = kmem_alloc(sizeof (*ie), KM_NOSLEEP)) == NULL)
	goto out;

	up = (upci_t *) arg1;
	ie->ie_dummy = 0;
	mutex_enter(&up->up_intx_inner_lk);
	list_insert_tail(&up->up_intx_int_list, ie);
	mutex_exit(&up->up_intx_inner_lk);
out:
	cmn_err(CE_CONT, "INTX handler fired\n");
	return (DDI_INTR_UNCLAIMED);
}

static int
upci_intx_disable(upci_t *up)
{
	if (up->up_flags & UPCI_DEVINFO_INT_ENABLED) {
		ddi_intr_remove_handler(up->up_intx_hdl);
		mutex_destroy(&up->up_intx_inner_lk);
		ddi_intr_free(up->up_intx_hdl);
		up->up_flags &= ~UPCI_DEVINFO_INT_ENABLED;
	}
	return (0);
}

static int
upci_intx_update(upci_t *up, int enable)
{
	int actual;
	uint_t pri;

	cmn_err(CE_CONT, "%s: enable = %d\n",
	    __func__, enable);

	/* Try to disable the interrupts */
	if (!enable)
		return upci_intx_disable(up);

	/* Interrupts are already enabled */
	if (up->up_flags & UPCI_DEVINFO_INT_ENABLED)
		return (0);


	if (ddi_intr_alloc(up->up_dip, &up->up_intx_hdl,
	    DDI_INTR_TYPE_FIXED, 0, 1, &actual, 0) != DDI_SUCCESS ||
	    actual != 1) {
		cmn_err(CE_CONT, "%s: Failed to allocate intx hdl\n", __func__);
		goto out;
	}

	if (ddi_intr_get_pri(up->up_intx_hdl, &pri) != DDI_SUCCESS ||
	    pri >= ddi_intr_get_hilevel_pri()) {
		cmn_err(CE_CONT, "%s: Failed to get intx pri\n", __func__);
		goto free_intr;
	}

	mutex_init(&up->up_intx_inner_lk, NULL, MUTEX_DRIVER, DDI_INTR_PRI(pri));

	if (ddi_intr_add_handler(up->up_intx_hdl, upci_intx_handler,
	    (caddr_t) up, 0) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "%s: Failed to add intx handlers\n", __func__);
		goto destroy_inner_mutex;
	}

	if (ddi_intr_enable(up->up_intx_hdl) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "%s: Failed to enable intx\n", __func__);
		goto remove_hdlr;
	}

	up->up_flags |= UPCI_DEVINFO_INT_ENABLED;
	return (0);

remove_hdlr:
	ddi_intr_remove_handler(up->up_intx_hdl);
destroy_inner_mutex:
	mutex_destroy(&up->up_intx_inner_lk);
free_intr:
	ddi_intr_free(up->up_intx_hdl);
out:
	return (-1);
}

static uint_t
upci_msi_handler(caddr_t arg1, caddr_t arg2)
{
	upci_t *up;
	upci_msi_int_ent_t *ie;

	/* Interrupt loss!! */
	if ((ie = kmem_alloc(sizeof (*ie), KM_NOSLEEP)) == NULL)
		goto out;


	up = (upci_t *) arg1;
	ie->ie_number = (intptr_t) arg2;

	mutex_enter(&up->up_msi_inner_lk);
	list_insert_tail(&up->up_msi_int_list, ie);
	mutex_exit(&up->up_msi_inner_lk);

	cv_signal(&up->up_msi_outer_cv);
out:
	cmn_err(CE_CONT, "MSI handler fired\n");
	return (DDI_INTR_CLAIMED);
}

static int
upci_msi_disable(upci_t *up)
{
	int i;
	if (up->up_flags & UPCI_DEVINFO_MSI_ENABLED) {
		if (ddi_intr_block_disable(up->up_msi_hdl,
		    up->up_msi_count) != DDI_SUCCESS) {
			return (-1);
		}


		for (i = 0; i < up->up_msi_count; i++) {
			ddi_intr_remove_handler(up->up_msi_hdl[i]);
			ddi_intr_free(up->up_msi_hdl[i]);
		}

		mutex_destroy(&up->up_msi_inner_lk);
		up->up_msi_count = 0;
		up->up_flags &= ~UPCI_DEVINFO_MSI_ENABLED;
	}

	return (0);
}

static int
upci_msi_update(upci_t *up, int enable, int count)
{
	int i;
	int actual;
	uint_t pri;

	cmn_err(CE_CONT, "%s: enable = %d, count = %d\n",
	    __func__, enable, count);

	/* We are disabling MSI */
	if (!enable)
		return upci_msi_disable(up);

	/* Same number of vectors. Nothing to do */
	if (up->up_msi_count == count)
		return (0);

	upci_msi_disable(up);

	if (count <= 0) {
		cmn_err(CE_CONT, "%s: count should be > 0\n", __func__);
		return (-1);
	}

	/* We are enabling MSI interrupts */
	if (ddi_intr_alloc(up->up_dip, up->up_msi_hdl, DDI_INTR_TYPE_MSI,
	    0, count, &actual, DDI_INTR_ALLOC_NORMAL) != DDI_SUCCESS ||
	    actual != count) {
		cmn_err(CE_CONT, "%s: Failed to allocate vectors\n", __func__);
		return (-1);
	}

	cmn_err(CE_CONT, "%s: Getting msi intr priority\n", __func__);
	if (ddi_intr_get_pri(up->up_msi_hdl[0], &pri) != DDI_SUCCESS ||
	    pri >= ddi_intr_get_hilevel_pri()) {
		cmn_err(CE_CONT, "%s: Failed to get msi intr priority\n", __func__);
		goto free_vector;
	}

	mutex_init(&up->up_msi_inner_lk, NULL, MUTEX_DRIVER, DDI_INTR_PRI(pri));

	cmn_err(CE_CONT, "%s: Adding intr handlers\n", __func__);
	for (i = 0; i < count; i++) {
		if (ddi_intr_add_handler(up->up_msi_hdl[i],
		    upci_msi_handler, (caddr_t) up,
		    (void *)((intptr_t) i)) != DDI_SUCCESS) {
			cmn_err(CE_CONT, "%s: Failed adding handlers\n", __func__);
			while (--i >= 0)
				ddi_intr_remove_handler(up->up_msi_hdl[i]);
			goto out2;
		}
	}

	cmn_err(CE_CONT, "%s: Successfully added interrupt handlers\n", __func__);
	if (ddi_intr_block_enable(up->up_msi_hdl, count) == DDI_SUCCESS) {
		cmn_err(CE_CONT, "%s: Successfully enabled block\n", __func__);
		up->up_msi_count = count;
		up->up_flags |= UPCI_DEVINFO_MSI_ENABLED;
		return (0);
	}
	cmn_err(CE_CONT, "%s: Failed to enable block\n", __func__);

	for (i = 0; i < count; i++) {
		ddi_intr_remove_handler(up->up_msi_hdl[i]);
	}
out2:
	mutex_destroy(&up->up_msi_inner_lk);

free_vector:
	for (i = 0; i < count; i++) {
		ddi_intr_free(up->up_msi_hdl[i]);
	}

	return (-1);
}

static int
upci_int_update_ioctl(dev_t dev,upci_int_update_t *arg, cred_t *cr, int *rv)
{
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;
	upci_int_update_t ip;

	if (copyin((void *) arg, &ip, sizeof(ip)) != 0) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}


	*rv = 0;
	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	mutex_enter(&up->up_msi_outer_lk);

	switch (ip.iu_type) {
	case UPCI_INTR_TYPE_FIXED:
		rval = *rv = upci_intx_update(up, ip.iu_enable);
	break;
	case UPCI_INTR_TYPE_MSI:
		rval = *rv = upci_msi_update(up, ip.iu_enable, ip.iu_vcount);
	break;
	default:
		rval = *rv = EINVAL;
		goto out;
	break;
	}
out:
	mutex_exit(&up->up_msi_outer_lk);

	return (abs(rval));
}

static int
upci_intx_get(upci_t *up, upci_int_get_t * arg)
{
	return (-1);
}

static int
upci_int_get_ioctl(dev_t dev, upci_int_get_t * arg, cred_t *cr, int *rv)
{
	int rval = DDI_SUCCESS;
	upci_t *up;
	minor_t instance;
	upci_int_get_t ig;
	upci_msi_int_ent_t *mie;
	upci_intx_int_ent_t *iie;

	if (copyin((void *) arg, &ig, sizeof(ig)) != 0) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	instance = getminor(dev);
	if ((up = ddi_get_soft_state(soft_state_p, instance)) == NULL) {
		rval = *rv = EINVAL;
		return (abs(rval));
	}

	*rv = 0;
	switch (ig.ig_type) {
	case UPCI_INTR_TYPE_FIXED:
		mutex_enter(&up->up_intx_outer_lk);
try_intx_outer:
		if (!(up->up_flags & UPCI_DEVINFO_INT_ENABLED)) {
wait_on_intx_outer:
			if (cv_wait_sig(&up->up_intx_outer_cv,
			    &up->up_intx_outer_lk) == 0) {
				mutex_exit(&up->up_intx_outer_lk);
				rval = *rv = EINVAL;
				return (abs(rval));
			}
			goto try_intx_outer;
		}
		rval = *rv = upci_intx_get(up, arg);

		/* INTX is enabled, we have the outer lock */
		mutex_enter(&up->up_intx_inner_lk);
		if (!list_is_empty(&up->up_intx_int_list)) {
			iie = list_remove_head(&up->up_intx_int_list);
			mutex_exit(&up->up_intx_inner_lk);
			mutex_exit(&up->up_intx_outer_lk);
			ig.ig_number = iie->ie_dummy;
			kmem_free(iie, sizeof(*iie));
			if (copyout((void *) &ig, arg,
			    sizeof (upci_int_get_t)) == 0) {
				return (0);
			}
			/* Intx loss */
			rval = *rv = EINVAL;
			return (abs(rval));
		}
		mutex_exit(&up->up_intx_inner_lk);
		goto wait_on_intx_outer;
	break;
	case UPCI_INTR_TYPE_MSI:

		mutex_enter(&up->up_msi_outer_lk);
try_msi_outer:
		if (!(up->up_flags & UPCI_DEVINFO_MSI_ENABLED)) {
wait_on_msi_outer:
			if (cv_wait_sig(&up->up_msi_outer_cv,
			    &up->up_msi_outer_lk) == 0) {
				/* Interrupted */
				mutex_exit(&up->up_msi_outer_lk);
				rval = *rv = EINVAL;
				return (abs(rval));
			}

			goto try_msi_outer;
		}

		/* MSI is enabled, we have the outer lock */
		mutex_enter(&up->up_msi_inner_lk);
		if (!list_is_empty(&up->up_msi_int_list)) {
			mie = list_remove_head(&up->up_msi_int_list);
			mutex_exit(&up->up_msi_inner_lk);
			mutex_exit(&up->up_msi_outer_lk);
			ig.ig_number = mie->ie_number;
			kmem_free(mie, sizeof(*mie));
			if (copyout((void *) &ig, arg,
			    sizeof (upci_int_get_t)) == 0) {
				return (0);
			}
			/* Interrupt loss!! */
			rval = *rv = EINVAL;
			return (abs(rval));
		}
		mutex_exit(&up->up_msi_inner_lk);

		/* No data found, wait on the outer lock */
		goto wait_on_msi_outer;
	break;
	default:
		rval = *rv = EINVAL;
	break;
	}

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
	case UPCI_IOCTL_DEV_INFO:
		rval = upci_devinfo_ioctl(dev,
		    (upci_dev_info_t *) arg, cr, rv);
	break;
	case UPCI_IOCTL_REG_INFO:
		rval = upci_get_reg_info_ioctl(dev,
		    (upci_reg_info_t *) arg, cr, rv);
	break;
	case UPCI_IOCTL_INT_UPDATE:
		rval = upci_int_update_ioctl(dev,
		    (upci_int_update_t *) arg, cr, rv);
	break;
	case UPCI_IOCTL_INT_GET:
		rval = upci_int_get_ioctl(dev,
		    (upci_int_get_t *) arg, cr, rv);
	break;
	case UPCI_IOCTL_XDMA_ALLOC:
		rval = upci_xdma_alloc(dev, (upci_dma_t *) arg, cr, rv);
	break;
	case UPCI_IOCTL_XDMA_REMOVE:
		rval = upci_xdma_remove(dev, (upci_dma_t *) arg, cr, rv);
	break;
	case UPCI_IOCTL_XDMA_RW:
		rval = upci_xdma_rw(dev, (upci_dma_t *) arg, cr, rv);
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

	mutex_init(&up->up_intx_outer_lk, NULL, MUTEX_DRIVER, NULL);
	cv_init(&up->up_intx_outer_cv, NULL, CV_DRIVER, NULL);
	list_create(&up->up_intx_int_list, sizeof (upci_intx_int_ent_t),
	    offsetof (upci_intx_int_ent_t, ie_next));

	up->up_msi_count = 0;
	mutex_init(&up->up_msi_outer_lk, NULL, MUTEX_DRIVER, NULL);
	cv_init(&up->up_msi_outer_cv, NULL, CV_DRIVER, NULL);
	list_create(&up->up_msi_int_list, sizeof (upci_msi_int_ent_t),
	    offsetof (upci_msi_int_ent_t, ie_next));

	list_create(&up->up_xdma_list, sizeof (upci_xdma_ent_t),
	    offsetof (upci_xdma_ent_t, xe_next));

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

	list_destroy(&up->up_xdma_list);

	ddi_remove_minor_node(dip, "upci");
	mutex_destroy(&up->up_lk);

	mutex_destroy(&up->up_intx_outer_lk);
	cv_destroy(&up->up_intx_outer_cv);
	list_destroy(&up->up_intx_int_list);

	mutex_destroy(&up->up_msi_outer_lk);
	cv_destroy(&up->up_msi_outer_cv);
	list_destroy(&up->up_msi_int_list);

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
