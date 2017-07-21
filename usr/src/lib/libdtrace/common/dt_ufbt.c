/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

#include <assert.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <alloca.h>
#include <libgen.h>
#include <stddef.h>
#include <sys/sysmacros.h>

#include <dt_impl.h>
#include <dt_program.h>
#include <dt_pid.h>
#include <dt_string.h>
#include <dt_module.h>


/* replace this with the proper ufbt
typedef struct dt_pid_probe {
	dtrace_hdl_t *dpp_dtp;
	dt_pcb_t *dpp_pcb;
	dt_proc_t *dpp_dpr;
	struct ps_prochandle *dpp_pr;
	const char *dpp_mod;
	char *dpp_func;
	const char *dpp_name;
	const char *dpp_obj;
	uintptr_t dpp_pc;
	size_t dpp_size;
	Lmid_t dpp_lmid;
	uint_t dpp_nmatches;
	uint64_t dpp_stret[4];
	GElf_Sym dpp_last;
	uint_t dpp_last_taken;
} dt_pid_probe_t;
*/

int
dt_ufbt_create_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
    dt_pcb_t *pcb, dt_proc_t *dpr)
{
	printf("we are creating probes\n");
	return 0;
}
