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


typedef struct upci_intx_int_ent_s {
	int		ie_dummy;
	list_node_t	ie_next;
} upci_intx_int_ent_t;

typedef struct upci_msi_int_ent_s {
	int		ie_number;
	list_node_t	ie_next;
} upci_msi_int_ent_t;

typedef struct upci_reg_s {
	uint32_t		reg_flags;
	caddr_t			reg_base;
	off_t			reg_size;
	ddi_acc_handle_t	reg_hdl;
} upci_reg_t;

typedef struct upci_ch_ent_s {
	size_t			ch_length;
	size_t			ch_real_length;
	caddr_t			ch_kaddr;
	ddi_dma_handle_t	ch_hdl;
	ddi_acc_handle_t	ch_acc_hdl;
	ddi_dma_cookie_t	ch_cookie;
	uint_t			ch_ncookies;
	list_node_t		ch_next;
} upci_ch_ent_t;

typedef struct upci_s {
	dev_info_t		*up_dip;
	ddi_acc_handle_t 	up_hdl;
	kmutex_t		up_lk;
	uint_t			up_flags;
	int			up_nregs;
	upci_reg_t		*up_regs;

	ddi_intr_handle_t	up_intx_hdl;
	kmutex_t		up_intx_outer_lk;
	kcondvar_t		up_intx_outer_cv;
	kmutex_t		up_intx_inner_lk;
	list_t			up_intx_int_list;

	ddi_intr_handle_t	up_msi_hdl[32];
	int			up_msi_count;
	kmutex_t		up_msi_outer_lk;
	kcondvar_t		up_msi_outer_cv;
	kmutex_t		up_msi_inner_lk;
	list_t			up_msi_int_list;

	list_t			up_ch_xdma_list;
} upci_t;


int upci_xdma_alloc_coherent(dev_t, upci_coherent_t *, cred_t *, int *);
int upci_xdma_free_coherent(dev_t, upci_coherent_t *, cred_t *, int *);
int upci_xdma_rw_coherent(dev_t, upci_coherent_t *, cred_t *, int *, int);
