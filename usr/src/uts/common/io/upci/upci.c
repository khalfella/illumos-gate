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


#include <sys/upci.h>

static dev_info_t *upci_dip;

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
upci_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
        int ret = DDI_FAILURE;

        switch (cmd) {
        case DDI_INFO_DEVT2DEVINFO:
                *resultp = upci_dip;
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
    minor_t instance;
    if (cmd != DDI_ATTACH)
        return (DDI_FAILURE);

    if (upci_dip != NULL)
        return (DDI_FAILURE);

    instance = ddi_get_instance(dip);
    if (ddi_create_minor_node(dip, "upci", S_IFCHR, instance,
        DDI_PSEUDO,  0) == DDI_FAILURE)
        return (DDI_FAILURE);

    upci_dip = dip;
    return (DDI_SUCCESS);
}

static int
upci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
    if (cmd != DDI_DETACH)
        return (DDI_FAILURE);

    VERIFY(dip == upci_dip);
    ddi_remove_minor_node(upci_dip, "upci");
    upci_dip = NULL;
    return (DDI_SUCCESS);
}

static struct cb_ops upci_cb_ops = {
    upci_open,        /* open */
    upci_close,       /* close */
    nulldev,        /* strategy */
    nulldev,        /* print */
    nodev,          /* dump */
    upci_read,        /* read */
    nodev,          /* write */
    nodev,          /* ioctl */
    nodev,          /* devmap */
    nodev,          /* mmap */
    nodev,          /* segmap */
    nochpoll,       /* poll */
    ddi_prop_op,        /* cb_prop_op */
    NULL,           /* streamtab  */
    D_MP            /* Driver compatibility flag */
};

static struct dev_ops upci_dev_ops = {
    DEVO_REV,       /* devo_rev */
    0,          /* refcnt */
    upci_getinfo,     /* get_dev_info */
    nulldev,        /* identify */
    nulldev,        /* probe */
    upci_attach,      /* attach */
    upci_detach,      /* detach */
    nodev,          /* reset */
    &upci_cb_ops,     /* driver operations */
    NULL,           /* bus operations */
    nodev,          /* dev power */
    ddi_quiesce_not_needed  /* quiesce */
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
    return (mod_remove(&upci_modlinkage));
}
