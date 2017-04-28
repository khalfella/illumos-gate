#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/kmem.h>
#include <sys/strsubr.h>
#include <sys/dtrace.h>
#include <sys/cyclic.h>
#include <sys/atomic.h>

static dev_info_t *ufbt_devi;
static dtrace_provider_id_t ufbt_id;

typedef struct ufbt_probe {
	dtrace_id_t	ufbt_id;
	cyclic_id_t	ufbt_cyclic;
} ufbt_probe_t;

static void
ufbt_tick(void *arg)
{
	ufbt_probe_t *ufbt = arg;
	dtrace_probe(ufbt->ufbt_id, CPU->cpu_profile_pc,
	    CPU->cpu_profile_upc, 0, 0, 0);
}

/*ARGSUSED*/
static void
ufbt_provide(void *arg, const dtrace_probedesc_t *desc)
{
	ufbt_probe_t *ufbt;

	if (dtrace_probe_lookup(ufbt_id, NULL, NULL, "ufbt-tick-1s") != 0)
		return;

	ufbt = kmem_zalloc(sizeof (ufbt_probe_t), KM_SLEEP);
	ufbt->ufbt_cyclic = CYCLIC_NONE;
	ufbt->ufbt_id = dtrace_probe_create(ufbt_id,
	    NULL, NULL, "ufbt-tick-1s", 0, ufbt);
}

/*ARGSUSED*/
static int
ufbt_enable(void *arg, dtrace_id_t id, void *parg)
{
	ufbt_probe_t *ufbt = parg;
	cyc_handler_t hdlr;
	cyc_time_t when;

	hdlr.cyh_func = ufbt_tick;
	hdlr.cyh_arg = ufbt;
	hdlr.cyh_level = CY_HIGH_LEVEL;

	when.cyt_interval = NANOSEC / SEC;
	when.cyt_when = dtrace_gethrtime() + when.cyt_interval;
	ufbt->ufbt_cyclic = cyclic_add(&hdlr, &when);

	return (0);
}

/*ARGSUSED*/
static void
ufbt_disable(void *arg, dtrace_id_t id, void *parg)
{
	ufbt_probe_t *ufbt = parg;
	cyclic_remove(ufbt->ufbt_cyclic);
	ufbt->ufbt_cyclic = CYCLIC_NONE;
}

/*ARGSUSED*/
static int
ufbt_mode(void *arg, dtrace_id_t id, void *parg)
{
	int mode;

	if (CPU->cpu_profile_pc != 0) {
		mode = DTRACE_MODE_KERNEL;
	} else {
		mode = DTRACE_MODE_USER;
	}

	mode |= DTRACE_MODE_NOPRIV_RESTRICT;

	return (mode);
}

/*ARGSUSED*/
static void
ufbt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	ufbt_probe_t *ufbt = parg;
	kmem_free(ufbt, sizeof (ufbt_probe_t));
}

static dtrace_pattr_t ufbt_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
};

static dtrace_pops_t ufbt_pops = {
	ufbt_provide,
	NULL,
	ufbt_enable,
	ufbt_disable,
	NULL,
	NULL,
	NULL,
	NULL,
	ufbt_mode,
	ufbt_destroy
};

static int
ufbt_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "ufbt", S_IFCHR, 0,
	    DDI_PSEUDO, NULL) == DDI_FAILURE ||
	    dtrace_register("ufbt", &ufbt_attr,
	    DTRACE_PRIV_KERNEL | DTRACE_PRIV_USER, NULL,
	    &ufbt_pops, NULL, &ufbt_id) != 0) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	ufbt_devi = devi;
	return (DDI_SUCCESS);
}

static int
ufbt_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (dtrace_unregister(ufbt_id) != 0)
		return (DDI_FAILURE);

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
ufbt_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)ufbt_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*ARGSUSED*/
static int
ufbt_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return (0);
}

static struct cb_ops ufbt_cb_ops = {
	ufbt_open,		/* open */
	nodev,			/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops ufbt_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ufbt_info,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	ufbt_attach,		/* attach */
	ufbt_detach,		/* detach */
	nodev,			/* reset */
	&ufbt_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	nodev,			/* dev power */
	ddi_quiesce_not_needed,		/* quiesce */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,		/* module type (this is a pseudo driver) */
	"Userland Function Boundary Tracing",	/* name of module */
	&ufbt_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}
