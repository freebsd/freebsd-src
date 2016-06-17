/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */
#ifndef _ASM_SN_XTALK_XTALK_PRIVATE_H
#define _ASM_SN_XTALK_XTALK_PRIVATE_H

#include <asm/sn/ioerror.h>        /* for error function and arg types */
#include <linux/devfs_fs_kernel.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/xtalk/xtalk.h>

/*
 * xtalk_private.h -- private definitions for xtalk
 * crosstalk drivers should NOT include this file.
 */

/*
 * All Crosstalk providers set up PIO using this information.
 */
struct xtalk_piomap_s {
    vertex_hdl_t            xp_dev;	/* a requestor of this mapping */
    xwidgetnum_t            xp_target;	/* target (node's widget number) */
    iopaddr_t               xp_xtalk_addr;	/* which crosstalk addr is mapped */
    size_t                  xp_mapsz;	/* size of this mapping */
    caddr_t                 xp_kvaddr;	/* kernel virtual address to use */
};

/*
 * All Crosstalk providers set up DMA using this information.
 */
struct xtalk_dmamap_s {
    vertex_hdl_t            xd_dev;	/* a requestor of this mapping */
    xwidgetnum_t            xd_target;	/* target (node's widget number) */
};

/*
 * All Crosstalk providers set up interrupts using this information.
 */
struct xtalk_intr_s {
    vertex_hdl_t            xi_dev;	/* requestor of this intr */
    xwidgetnum_t            xi_target;	/* master's widget number */
    xtalk_intr_vector_t     xi_vector;	/* 8-bit interrupt vector */
    iopaddr_t               xi_addr;	/* xtalk address to generate intr */
    void                   *xi_sfarg;	/* argument for setfunc */
    xtalk_intr_setfunc_t    xi_setfunc;		/* device's setfunc routine */
};

/*
 * Xtalk interrupt handler structure access functions
 */
#define	xtalk_intr_arg(xt)	((xt)->xi_sfarg)

#define	xwidget_hwid_is_sn0_xswitch(_hwid)	\
		(((_hwid)->part_num == XBOW_WIDGET_PART_NUM ) &&  	\
		 ((_hwid)->mfg_num == XBOW_WIDGET_MFGR_NUM ))

#define	xwidget_hwid_is_sn1_xswitch(_hwid)	\
		(((_hwid)->part_num == XXBOW_WIDGET_PART_NUM ||		\
		  (_hwid)->part_num == PXBOW_WIDGET_PART_NUM) &&  	\
		 ((_hwid)->mfg_num == XXBOW_WIDGET_MFGR_NUM ))

#define	xwidget_hwid_is_xswitch(_hwid)	\
			xwidget_hwid_is_sn1_xswitch(_hwid)

/* common iograph info for all widgets,
 * stashed in FASTINFO of widget connection points.
 */
struct xwidget_info_s {
    char                   *w_fingerprint;
    vertex_hdl_t            w_vertex;	/* back pointer to vertex */
    xwidgetnum_t            w_id;	/* widget id */
    struct xwidget_hwid_s   w_hwid;	/* hardware identification (part/rev/mfg) */
    vertex_hdl_t            w_master;	/* CACHED widget's master */
    xwidgetnum_t            w_masterid;		/* CACHED widget's master's widgetnum */
    error_handler_f        *w_efunc;	/* error handling function */
    error_handler_arg_t     w_einfo;	/* first parameter for efunc */
    char		   *w_name;	/* canonical hwgraph name */	
};

extern char             widget_info_fingerprint[];

#endif				/* _ASM_SN_XTALK_XTALK_PRIVATE_H */
