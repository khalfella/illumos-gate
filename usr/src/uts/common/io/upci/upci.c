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
#include <sys/avl.h>

#include <sys/pci_impl.h>
#include <sys/ddi_isa.h>

#include <sys/upci.h>

#define	abs(a) ((a) < 0 ? -(a) : (a))

typedef struct upci_s {
	char			up_devpath[MAXPATHLEN];
	dev_info_t		*up_dip;
	ddi_acc_handle_t 	up_hdl;
	kmutex_t		up_lk;
	uint64_t		up_flags;
	avl_node_t		up_avl;
} upci_t;

static avl_tree_t	*upci_devices;		/* Tree of devices under upci */
static kmutex_t		upci_devices_lk;	/* Tree lock */

static int
upci_devices_cmp(const void *l, const void *r) {
	upci_t *lpci = (upci_t *)l;
	upci_t *rpci = (upci_t *)r;
	int cmp = strncmp(lpci->up_devpath, rpci->up_devpath, MAXPATHLEN);

	if (cmp != 0)
		return cmp/abs(cmp);
	return (0);
}

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
upci_list_devices_ioctl(upci_t *up, upci_cmd_t *ucmd, upci_cmd_t *ucmdup,
    cred_t *cr, int *rv)
{
	upci_t *pent;
	upci_devinfo_t *pdi, *pudi;
	int rval = DDI_SUCCESS;
	unsigned long devcnt;

	*rv = 0;
	pdi = kmem_alloc(sizeof (*pdi), KM_SLEEP);
	mutex_enter(&upci_devices_lk);
	devcnt = avl_numnodes(upci_devices);
	if (ucmd->cm_uobufsz < (devcnt * sizeof (upci_devinfo_t))) {
		rval = *rv = EINVAL;
		goto out;
	}

	pudi = (upci_devinfo_t *) ucmd->cm_uobuff;

	for (pent = avl_first(upci_devices); pent != NULL;
	    pent = AVL_NEXT(upci_devices, pent)) {
		strcpy(pdi->di_devpath, pent->up_devpath);
		pdi->di_flags = pent->up_flags;
		if (copyout(pdi, pudi++, sizeof (upci_devinfo_t)) != 0) {
			rval = *rv = EINVAL;
			goto out;
		}
	}

	uint64_t uobufsz = devcnt * sizeof (upci_devinfo_t);
	if (copyout(&uobufsz, &ucmdup->cm_uobufsz, sizeof (uint64_t)) != 0) {
		rval = *rv = EINVAL;
	}
out:
	mutex_exit(&upci_devices_lk);
	kmem_free(pdi, sizeof (*pdi));
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
	case UPCI_IOCTL_LIST_DEVICES:
		rval = upci_list_devices_ioctl(NULL, ucmd, (upci_cmd_t *)arg,
		    cr, rv);
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

	up = kmem_zalloc(sizeof (*up), KM_SLEEP);

	if (pci_config_setup(dip, &up->up_hdl) != DDI_SUCCESS) {
		kmem_free(up, sizeof (*up));
		ddi_remove_minor_node(dip, "upci");
		cmn_err(CE_CONT, "Failed to access PCI config space\n");
		return (DDI_FAILURE);
	}

	mutex_init(&up->up_lk, NULL, MUTEX_DRIVER, NULL);

	up->up_dip = dip;
	ddi_pathname(dip, up->up_devpath);

	mutex_enter(&upci_devices_lk);
	avl_add(upci_devices, up);
	mutex_exit(&upci_devices_lk);

	ddi_set_driver_private(dip, (caddr_t)up);

	cmn_err(CE_CONT, "Attached \"%s\"\n", up->up_devpath);

	return (DDI_SUCCESS);
}

static int
upci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	upci_t *up;

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	/* Try to find this device in the tree */
	mutex_enter(&upci_devices_lk);
	for (up = avl_first(upci_devices);
	    up != NULL && up->up_dip == dip;
	    up = AVL_NEXT(upci_devices, up))
		;

	if (up == NULL) {
		mutex_exit(&upci_devices_lk);
		return (DDI_FAILURE);
	}

	avl_remove(upci_devices, up);
	mutex_exit(&upci_devices_lk);

	ddi_remove_minor_node(dip, "upci");
	pci_config_teardown(&up->up_hdl);

	mutex_destroy(&up->up_lk);
	kmem_free(up, sizeof(*up));

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
	upci_devices = kmem_zalloc(sizeof (*upci_devices), KM_SLEEP);
	avl_create(upci_devices, upci_devices_cmp, sizeof (upci_t),
		    offsetof (upci_t, up_avl));

	mutex_init(&upci_devices_lk, NULL, MUTEX_DRIVER, NULL);

	cmn_err(CE_CONT, "Initialized upci_devices to %p\n", upci_devices);

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
	if (upci_devices != NULL) {
		VERIFY(avl_is_empty(upci_devices));
		avl_destroy(upci_devices);
		kmem_free((caddr_t) upci_devices, sizeof (*upci_devices));
		mutex_destroy(&upci_devices_lk);
		cmn_err(CE_CONT, "upci fini\n");
	}

	return (mod_remove(&upci_modlinkage));
}
