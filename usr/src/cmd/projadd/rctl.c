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
#include "rctl.h"



sig_t sigs[SIGS_CNT] = {
	/* Signal names */
	{"ABRT", RCTL_SIG_ABRT},
	{"XRES", RCTL_SIG_XRES},
	{"HUP",  RCTL_SIG_HUP},
	{"STOP", RCTL_SIG_STOP},
	{"TERM", RCTL_SIG_TERM},
	{"KILL", RCTL_SIG_KILL},
	{"XFSZ", RCTL_SIG_XFSZ},
	{"XCPU", RCTL_SIG_XCPU},

	/* Singnal numbers */
	{"6",  RCTL_SIG_ABRT},
	{"38", RCTL_SIG_XRES},
	{"1",  RCTL_SIG_HUP},
	{"23", RCTL_SIG_STOP},
	{"15", RCTL_SIG_TERM},
	{"9",  RCTL_SIG_KILL},
	{"31", RCTL_SIG_XFSZ},
	{"30", RCTL_SIG_XCPU},
};


int
rctl_pool_exist(char *name)
{
	pool_conf_t *conf;
	pool_t *pool;
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

	pool = pool_get_pool(conf, name);
	if (pool == NULL) {
		pool_conf_close(conf);
		pool_conf_free(conf);
		return (1);
	}

	pool_conf_close(conf);
	pool_conf_free(conf);
	return (0);
}

int
rctl_get_info(char *name, rctl_info_t *pinfo)
{
	rctlblk_t *blk1, *blk2, *tmp;
	rctl_priv_t priv;

	blk1 = blk2 = tmp = NULL;
	blk1 = util_safe_malloc(rctlblk_size());
	blk2 = util_safe_malloc(rctlblk_size());
	int ret = 1;

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
rctl_get_rule(rctl_info_t *pinfo, rctlrule_t* prule)
{

	prule->rtcl_max = pinfo->value;
	if (pinfo->flags & RCTL_GLOBAL_BYTES) {
		prule->rtcl_type = RCTL_TYPE_BYTES;
	} else if (pinfo->flags & RCTL_GLOBAL_SECONDS) {
		prule->rtcl_type = RCTL_TYPE_SCNDS;
	} else if (pinfo->flags & RCTL_GLOBAL_COUNT) {
		prule->rtcl_type = RCTL_TYPE_COUNT;
	} else {
		prule->rtcl_type = RCTL_TYPE_UNKWN;
	}

	if (pinfo->flags & RCTL_GLOBAL_NOBASIC) {
		prule->rctl_privs = RCTL_PRIV_PRIVE | RCTL_PRIV_PRIVD;
	} else {
		prule->rctl_privs = RCTL_PRIV_ALLPR;
	}

	if (pinfo->flags & RCTL_GLOBAL_DENY_ALWAYS) {
		prule->rctl_action = RCTL_ACTN_DENY;
	} else if (pinfo->flags & RCTL_GLOBAL_DENY_NEVER) {
		prule->rctl_action = RCTL_ACTN_NONE;
	} else {
		prule->rctl_action = RCTL_ACTN_NONE | RCTL_ACTN_DENY;
	}

	if (pinfo->flags & RCTL_GLOBAL_SIGNAL_NEVER) {
		prule->rctl_sigs = 0;
	} else {
		prule->rctl_action |= RCTL_ACTN_SIGN;
		prule->rctl_sigs = RCTL_SIG_CMN;
		if (pinfo->flags & RCTL_GLOBAL_CPU_TIME) {
			prule->rctl_sigs |= RCTL_SIG_XCPU;
		}
		if (pinfo->flags & RCTL_GLOBAL_FILE_SIZE) {
			prule->rctl_sigs |= RCTL_SIG_XFSZ;
		}
	}
}
