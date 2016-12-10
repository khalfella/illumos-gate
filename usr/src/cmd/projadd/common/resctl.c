#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <stddef.h>
#include <limits.h>
#include <rctl.h>
#include <regex.h>
#include <ctype.h>
#include <zone.h>
#include <pool.h>
#include <sys/pool_impl.h>
#include <unistd.h>
#include <stropts.h>

#include "util.h"
#include "resctl.h"

sig_t sigs[SIGS_CNT] = {
	/* Signal names */
	{"ABRT", RESCTL_SIG_ABRT},
	{"XRES", RESCTL_SIG_XRES},
	{"HUP",  RESCTL_SIG_HUP},
	{"STOP", RESCTL_SIG_STOP},
	{"TERM", RESCTL_SIG_TERM},
	{"KILL", RESCTL_SIG_KILL},
	{"XFSZ", RESCTL_SIG_XFSZ},
	{"XCPU", RESCTL_SIG_XCPU},

	/* Singnal numbers */
	{"6",  RESCTL_SIG_ABRT},
	{"38", RESCTL_SIG_XRES},
	{"1",  RESCTL_SIG_HUP},
	{"23", RESCTL_SIG_STOP},
	{"15", RESCTL_SIG_TERM},
	{"9",  RESCTL_SIG_KILL},
	{"31", RESCTL_SIG_XFSZ},
	{"30", RESCTL_SIG_XCPU},
};

/*
 * Check the existance of a resource pool in the system
 */
int
resctl_pool_exist(char *name)
{
	pool_conf_t *conf;
	pool_status_t status;
	int fd;

	/*
	 * Determine if pools are enabled using /dev/pool, as
	 * libpool may not be present.
	 */
	if (getzoneid() != GLOBAL_ZONEID ||
	    (fd = open("/dev/pool", O_RDONLY)) < 0) {
		return (1);
	}

	if (ioctl(fd, POOL_STATUS, &status) < 0) {
		(void) close(fd);
		return (1);
	}

	(void) close(fd);

	if (status.ps_io_state != 1)
		return (1);

	/* If pools are enabled, assume libpool is present. */
	if ((conf = pool_conf_alloc()) == NULL)
		return (1);

	if (pool_conf_open(conf, pool_dynamic_location(), PO_RDONLY)) {
		pool_conf_free(conf);
		return (1);
	}

	if (pool_get_pool(conf, name) == NULL) {
		(void) pool_conf_close(conf);
		pool_conf_free(conf);
		return (1);
	}

	(void) pool_conf_close(conf);
	pool_conf_free(conf);
	return (0);
}

int
resctl_get_info(char *name, resctl_info_t *pinfo)
{
	rctlblk_t *blk1, *blk2, *tmp;
	rctl_priv_t priv;
	int ret = 1;

	blk1 = blk2 = tmp = NULL;
	blk1 = util_safe_malloc(rctlblk_size());
	blk2 = util_safe_malloc(rctlblk_size());

	if (getrctl(name, NULL, blk1, RCTL_FIRST) == 0) {
		priv = rctlblk_get_privilege(blk1);
		while (priv != RCPRIV_SYSTEM) {
			tmp = blk2;
			blk2 = blk1;
			blk1 = tmp;
			if (getrctl(name, blk2, blk1, RCTL_NEXT) != 0) {
				goto out;
			}
			priv = rctlblk_get_privilege(blk1);
		}

		pinfo->value = rctlblk_get_value(blk1);
		pinfo->flags = rctlblk_get_global_flags(blk1);
		ret = 0;
	}

out:
	free(blk1);
	free(blk2);
	return (ret);
}

void
resctl_get_rule(resctl_info_t *pinfo, resctlrule_t* prule)
{

	prule->resctl_max = pinfo->value;
	if (pinfo->flags & RCTL_GLOBAL_BYTES) {
		prule->resctl_type = RESCTL_TYPE_BYTES;
	} else if (pinfo->flags & RCTL_GLOBAL_SECONDS) {
		prule->resctl_type = RESCTL_TYPE_SCNDS;
	} else if (pinfo->flags & RCTL_GLOBAL_COUNT) {
		prule->resctl_type = RESCTL_TYPE_COUNT;
	} else {
		prule->resctl_type = RESCTL_TYPE_UNKWN;
	}

	if (pinfo->flags & RCTL_GLOBAL_NOBASIC) {
		prule->resctl_privs = RESCTL_PRIV_PRIVE | RESCTL_PRIV_PRIVD;
	} else {
		prule->resctl_privs = RESCTL_PRIV_ALLPR;
	}

	if (pinfo->flags & RCTL_GLOBAL_DENY_ALWAYS) {
		prule->resctl_action = RESCTL_ACTN_DENY;
	} else if (pinfo->flags & RCTL_GLOBAL_DENY_NEVER) {
		prule->resctl_action = RESCTL_ACTN_NONE;
	} else {
		prule->resctl_action = RESCTL_ACTN_NONE | RESCTL_ACTN_DENY;
	}

	if (pinfo->flags & RCTL_GLOBAL_SIGNAL_NEVER) {
		prule->resctl_sigs = 0;
	} else {
		prule->resctl_action |= RESCTL_ACTN_SIGN;
		prule->resctl_sigs = RESCTL_SIG_CMN;
		if (pinfo->flags & RCTL_GLOBAL_CPU_TIME) {
			prule->resctl_sigs |= RESCTL_SIG_XCPU;
		}
		if (pinfo->flags & RCTL_GLOBAL_FILE_SIZE) {
			prule->resctl_sigs |= RESCTL_SIG_XFSZ;
		}
	}
}
