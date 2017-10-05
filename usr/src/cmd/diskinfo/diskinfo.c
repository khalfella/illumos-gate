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
 * Copyright (c) 2013 Joyent Inc., All rights reserved.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <strings.h>

#include <libdiskmgt.h>
#include <sys/nvpair.h>
#include <sys/param.h>
#include <sys/ccompile.h>
#include <sys/avl.h>

#include <fm/libtopo.h>
#include <fm/topo_hc.h>
#include <fm/topo_list.h>
#include <sys/fm/protocol.h>
#include <modules/common/disk/disk.h>

static boolean_t g_cflag = B_FALSE;	/* Condensed */
static boolean_t g_Hflag = B_FALSE;	/* Scripted  */
static boolean_t g_Pflag = B_FALSE;	/* Physical  */
static boolean_t g_pflag = B_FALSE;	/* Parseable */

typedef struct di_phys {
	uint64_t dp_size;
	uint32_t dp_blksize;
	char *dp_vid;
	char *dp_pid;
	boolean_t dp_removable;
	boolean_t dp_ssd;
	char *dp_dev;
	char *dp_ctype;
	char *dp_serial;
	char *dp_slotname;
	int dp_chassis;
	int dp_slot;
	int dp_faulty;
	int dp_locate;
	avl_node_t dp_tnode;
} di_phys_t;

static avl_tree_t g_disks;

static int
avl_di_comp(const void *di1, const void *di2)
{
	int cmp;
	const di_phys_t *di1p = di1;
	const di_phys_t *di2p = di2;
	if (di1p->dp_dev == NULL || di2p->dp_dev == NULL) {
		if (di1p->dp_chassis != di2p->dp_chassis) {
			if (di1p->dp_chassis < di2p->dp_chassis)
				return (-1);
			return (1);
		}
		if (di1p->dp_slot != di2p->dp_slot) {
			if (di1p->dp_slot < di2p->dp_slot)
				return (-1);
			return (1);
		}
		if (di1p->dp_slotname != NULL && di2p->dp_slotname != NULL) {
			cmp = strcmp(di1p->dp_slotname, di2p->dp_slotname);
			if (cmp == 0)
				return (0);
			return (cmp/abs(cmp));
		}
		return (0);
	}
	cmp = strcmp(di1p->dp_dev, di2p->dp_dev);
	if (cmp == 0)
		return (0);
	return (cmp/abs(cmp));
}

static void __NORETURN
fatal(int rv, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(rv);
}

static void
usage(const char *execname)
{
	(void) fprintf(stderr, "Usage: %s [-Hp] [{-c|-P}]\n", execname);
}

static void
nvlist_query_string(nvlist_t *nvl, const char *label, char **val)
{
	if (nvlist_lookup_string(nvl, label, val) != 0)
		*val = "-";
}

static const char *
display_string(const char *label)
{
	return ((label) ? label : "-");
}

static const char *
display_tristate(int val)
{
	if (val == 0)
		return ("no");
	if (val == 1)
		return ("yes");

	return ("-");
}

static char
condensed_tristate(int val, char c)
{
	if (val == 0)
		return ('-');
	if (val == 1)
		return (c);

	return ('?');
}

static int
bay_walker(topo_hdl_t *hp, tnode_t *np, void *arg)
{
	int err;
	di_phys_t *dip;
	tnode_t *pnp;
	char *slotname;
	boolean_t found = B_FALSE;
	int slot, chassis;

	if (strcmp(topo_node_name(np), BAY) != 0)
		return (TOPO_WALK_NEXT);

	pnp = topo_node_parent(np);
	slot = topo_node_instance(np);
	chassis = topo_node_instance(pnp);

	for (dip = avl_first(&g_disks); dip != NULL;
	    dip = AVL_NEXT(&g_disks, dip)) {
		if (dip->dp_slot == slot && dip->dp_chassis == chassis) {
			found = B_TRUE;
			break;
		}
	}

	if (!found) {
		dip = malloc(sizeof (di_phys_t));
		assert(dip != NULL);
		memset(dip, 0, sizeof (di_phys_t));
		dip->dp_slot = slot;
		dip->dp_chassis = chassis;

		if (topo_prop_get_string(pnp, TOPO_PGROUP_PROTOCOL,
		    TOPO_PROP_LABEL, &slotname, &err) == 0) {
			dip->dp_slotname = strdup(slotname);
		}
		avl_add(&g_disks, dip);
	}

	return (TOPO_WALK_NEXT);
}

static int
disk_walker(topo_hdl_t *hp, tnode_t *np, void *arg)
{
	di_phys_t di;
	di_phys_t *dip;
	tnode_t *pnp;
	tnode_t *ppnp;
	topo_faclist_t fl;
	topo_faclist_t *lp;
	int err;
	topo_led_state_t mode;
	topo_led_type_t type;

	if (strcmp(topo_node_name(np), DISK) != 0)
		return (TOPO_WALK_NEXT);

	if (topo_prop_get_string(np, TOPO_PGROUP_STORAGE,
	    TOPO_STORAGE_LOGICAL_DISK_NAME, &di.dp_dev, &err) != 0) {
		return (TOPO_WALK_NEXT);
	}

	if ((dip = avl_find(&g_disks, &di, NULL)) == NULL)
		return (TOPO_WALK_NEXT);

	if (topo_prop_get_string(np, TOPO_PGROUP_STORAGE,
	    TOPO_STORAGE_SERIAL_NUM, &dip->dp_serial, &err) == 0) {
		dip->dp_serial = strdup(dip->dp_serial);
	}

	pnp = topo_node_parent(np);
	ppnp = topo_node_parent(pnp);
	if (strcmp(topo_node_name(pnp), BAY) == 0) {
		if (topo_node_facility(hp, pnp, TOPO_FAC_TYPE_INDICATOR,
		    TOPO_FAC_TYPE_ANY, &fl, &err) == 0) {
			for (lp = topo_list_next(&fl.tf_list); lp != NULL;
			    lp = topo_list_next(lp)) {
				uint32_t prop;

				if (topo_prop_get_uint32(lp->tf_node,
				    TOPO_PGROUP_FACILITY, TOPO_FACILITY_TYPE,
				    &prop, &err) != 0) {
					continue;
				}
				type = (topo_led_type_t)prop;

				if (topo_prop_get_uint32(lp->tf_node,
				    TOPO_PGROUP_FACILITY, TOPO_LED_MODE,
				    &prop, &err) != 0) {
					continue;
				}
				mode = (topo_led_state_t)prop;

				switch (type) {
				case TOPO_LED_TYPE_SERVICE:
					dip->dp_faulty = mode ? 1 : 0;
					break;
				case TOPO_LED_TYPE_LOCATE:
					dip->dp_locate = mode ? 1 : 0;
					break;
				default:
					break;
				}
			}
		}

		if (topo_prop_get_string(pnp, TOPO_PGROUP_PROTOCOL,
		    TOPO_PROP_LABEL, &dip->dp_slotname, &err) == 0) {
			dip->dp_slotname = strdup(dip->dp_slotname);
		}

		dip->dp_slot = topo_node_instance(pnp);
	}

	dip->dp_chassis = topo_node_instance(ppnp);

	return (TOPO_WALK_NEXT);
}

static void
enumerate_disks()
{
	topo_hdl_t *hp;
	topo_walk_t *wp;
	dm_descriptor_t *media;
	int err, i;
	int filter[] = { DM_DT_FIXED, -1 };
	dm_descriptor_t *disk, *controller;
	nvlist_t *mattrs, *dattrs, *cattrs = NULL;

	char *s, *c;
	di_phys_t *dip;
	size_t len;

	avl_create(&g_disks, avl_di_comp, sizeof (di_phys_t),
	    offsetof(di_phys_t, dp_tnode));

	err = 0;
	if ((media = dm_get_descriptors(DM_MEDIA, filter, &err)) == NULL) {
		fatal(-1, "failed to obtain media descriptors: %s\n",
		    strerror(err));
	}

	for (i = 0; media != NULL && media[i] != NULL; i++) {
		if ((disk = dm_get_associated_descriptors(media[i],
		    DM_DRIVE, &err)) == NULL) {
			continue;
		}

		dip = malloc(sizeof (di_phys_t));
		assert(dip != NULL);
		memset(dip, 0, sizeof (di_phys_t));

		mattrs = dm_get_attributes(media[i], &err);
		err = nvlist_lookup_uint64(mattrs, DM_SIZE, &dip->dp_size);
		assert(err == 0);
		err = nvlist_lookup_uint32(mattrs, DM_BLOCKSIZE,
		    &dip->dp_blksize);
		assert(err == 0);
		nvlist_free(mattrs);

		dattrs = dm_get_attributes(disk[0], &err);

		nvlist_query_string(dattrs, DM_VENDOR_ID, &dip->dp_vid);
		nvlist_query_string(dattrs, DM_PRODUCT_ID, &dip->dp_pid);
		nvlist_query_string(dattrs, DM_OPATH, &dip->dp_dev);

		dip->dp_vid = strdup(dip->dp_vid);
		dip->dp_pid = strdup(dip->dp_pid);
		dip->dp_dev = strdup(dip->dp_dev);

		dip->dp_removable = B_FALSE;
		if (nvlist_lookup_boolean(dattrs, DM_REMOVABLE) == 0)
			dip->dp_removable = B_TRUE;

		dip->dp_ssd = B_FALSE;
		if (nvlist_lookup_boolean(dattrs, DM_SOLIDSTATE) == 0)
			dip->dp_ssd = B_TRUE;

		nvlist_free(dattrs);

		if ((controller = dm_get_associated_descriptors(disk[0],
		    DM_CONTROLLER, &err)) != NULL) {
			cattrs = dm_get_attributes(controller[0], &err);
			nvlist_query_string(cattrs, DM_CTYPE, &dip->dp_ctype);
			dip->dp_ctype = strdup(dip->dp_ctype);
			for (c = dip->dp_ctype; *c != '\0'; c++)
				*c = toupper(*c);
			nvlist_free(cattrs);
		}

		if (dip->dp_ctype == NULL)
			dip->dp_ctype = strdup("");

		dm_free_descriptors(controller);
		dm_free_descriptors(disk);

		/*
		 * Parse full device path to only show the device name,
		 * i.e. c0t1d0.  Many paths will reference a particular
		 * slice (c0t1d0s0), so remove the slice if present.
		 */
		if ((c = strrchr(dip->dp_dev, '/')) != NULL) {
			s = dip->dp_dev;
			while ((*s++ = *++c))
				;
		}
		len = strlen(dip->dp_dev);
		if (dip->dp_dev[len - 2] == 's' &&
		    dip->dp_dev[len - 1] >= '0' &&
		    dip->dp_dev[len - 1] <= '9')
			dip->dp_dev[len - 2] = '\0';

		dip->dp_faulty = dip->dp_locate = -1;
		dip->dp_chassis = dip->dp_slot = -1;
		avl_add(&g_disks, dip);
	}

	dm_free_descriptors(media);


	/*
	 * Walk toplogy information to populate serial, chassis,
	 * slot, faulty and locator information.
	 */

	err = 0;
	hp = topo_open(TOPO_VERSION, NULL, &err);
	if (hp == NULL) {
		fatal(-1, "unable to obtain topo handle: %s",
		    topo_strerror(err));
	}

	err = 0;
	(void) topo_snap_hold(hp, NULL, &err);
	if (err != 0) {
		fatal(-1, "unable to hold topo snapshot: %s",
		    topo_strerror(err));
	}


	wp = topo_walk_init(hp, FM_FMRI_SCHEME_HC, disk_walker, NULL, &err);
	if (wp == NULL) {
		fatal(-1, "unable to initialise topo walker: %s",
		    topo_strerror(err));
	}

	while ((err = topo_walk_step(wp, TOPO_WALK_CHILD)) == TOPO_WALK_NEXT)
		;
	if (err == TOPO_WALK_ERR)
		fatal(-1, "topo walk failed");

	topo_walk_fini(wp);


	/* Here the aflag should work */
	wp = topo_walk_init(hp, FM_FMRI_SCHEME_HC, bay_walker, NULL, &err);
	if (wp == NULL) {
		fatal(-1, "unable to initialise topo walker: %s",
		    topo_strerror(err));
	}

	while ((err = topo_walk_step(wp, TOPO_WALK_CHILD)) == TOPO_WALK_NEXT)
		;
	if (err == TOPO_WALK_ERR)
		fatal(-1, "topo walk failed");

	topo_walk_fini(wp);

	topo_snap_release(hp);
	topo_close(hp);
}

static void
show_disks()
{
	uint64_t total;
	double total_in_GiB;
	char *sizestr = NULL, *slotname = NULL, *statestr = NULL;
	di_phys_t *dip;

	for (dip = avl_first(&g_disks); dip != NULL;
	    dip = AVL_NEXT(&g_disks, dip)) {
		/*
		 * The size is given in blocks, so multiply the number
		 * of blocks by the block size to get the total size,
		 * then convert to GiB.
		 */
		total = dip->dp_size * dip->dp_blksize;

		if (g_pflag) {
			(void) asprintf(&sizestr, "%llu", total);
		} else {
			total_in_GiB = (double)total / 1024.0 / 1024.0 / 1024.0;
			(void) asprintf(&sizestr, "%7.2f GiB", total_in_GiB);
		}

		if (g_pflag) {
			(void) asprintf(&slotname, "%d,%d",
			    dip->dp_chassis, dip->dp_slot);
		} else if (dip->dp_slotname != NULL) {
			(void) asprintf(&slotname, "[%d] %s",
			    dip->dp_chassis, dip->dp_slotname);
		} else {
			slotname = strdup("-");
		}

		if (g_cflag) {
			(void) asprintf(&statestr, "%c%c%c%c",
			    condensed_tristate(dip->dp_faulty, 'F'),
			    condensed_tristate(dip->dp_locate, 'L'),
			    condensed_tristate(dip->dp_removable, 'R'),
			    condensed_tristate(dip->dp_ssd, 'S'));
		}

		if (g_Pflag) {
			if (g_Hflag) {
				printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
				    dip->dp_dev, dip->dp_vid, dip->dp_pid,
				    display_string(dip->dp_serial),
				    display_tristate(dip->dp_faulty),
				    display_tristate(dip->dp_locate), slotname);
			} else {
				printf("%-22s  %-8s %-16s "
				    "%-20s %-3s %-3s %s\n",
				    dip->dp_dev, dip->dp_vid, dip->dp_pid,
				    display_string(dip->dp_serial),
				    display_tristate(dip->dp_faulty),
				    display_tristate(dip->dp_locate), slotname);
			}
		} else if (g_cflag) {
			if (g_Hflag) {
				printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
				    dip->dp_ctype, dip->dp_dev,
				    dip->dp_vid, dip->dp_pid,
				    display_string(dip->dp_serial),
				    sizestr, statestr, slotname);
			} else {
				printf("%-7s %-22s  %-8s %-16s "
				    "%-20s\n\t%-13s %-4s %s\n",
				    dip->dp_ctype, dip->dp_dev,
				    dip->dp_vid, dip->dp_pid,
				    display_string(dip->dp_serial),
				    sizestr, statestr, slotname);
			}
		} else {
			if (g_Hflag) {
				printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
				    dip->dp_ctype, dip->dp_dev,
				    dip->dp_vid, dip->dp_pid, sizestr,
				    display_tristate(dip->dp_removable),
				    display_tristate(dip->dp_ssd));
			} else {
				printf("%-7s %-22s  %-8s %-16s "
				    "%-13s %-3s %-3s\n",
				    dip->dp_ctype, dip->dp_dev,
				    dip->dp_vid, dip->dp_pid, sizestr,
				    display_tristate(dip->dp_removable),
				    display_tristate(dip->dp_ssd));
			}
		}
	free(sizestr); free(slotname); free(statestr);
	sizestr = slotname = statestr = NULL;
	}
}

int
main(int argc, char *argv[])
{
	char c;

	while ((c = getopt(argc, argv, ":cHPp")) != EOF) {
		switch (c) {
		case 'c':
			g_cflag = B_TRUE;
			break;
		case 'H':
			g_Hflag = B_TRUE;
			break;
		case 'P':
			g_Pflag = B_TRUE;
			break;
		case 'p':
			g_pflag = B_TRUE;
			break;
		case '?':
			usage(argv[0]);
			fatal(1, "unknown option -%c\n", optopt);
		default:
			fatal(-1, "unexpected error on option -%c\n", optopt);
		}
	}

	if (g_cflag && g_Pflag) {
		usage(argv[0]);
		fatal(1, "-c and -P are mutually exclusive\n");
	}

	if (!g_Hflag) {
		if (g_pflag) {
			printf("DISK                    VID      PID"
			    "              SERIAL               FLT LOC"
			    " LOCATION\n");
		} else if (g_cflag) {
			printf("TYPE    DISK                    VID      PID"
			    "              SERIAL\n");
			printf("\tSIZE          FLRS LOCATION\n");
		} else {
			printf("TYPE    DISK                    VID      PID"
			    "              SIZE          RMV SSD\n");
		}
	}

	enumerate_disks();
	show_disks();

	return (0);
}
