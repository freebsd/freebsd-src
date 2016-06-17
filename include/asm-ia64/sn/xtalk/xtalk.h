/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */
#ifndef _ASM_SN_XTALK_XTALK_H
#define _ASM_SN_XTALK_XTALK_H
#include <linux/config.h>

#ifdef __KERNEL__
#include "asm/sn/sgi.h"
#endif


/*
 * xtalk.h -- platform-independent crosstalk interface
 */
/*
 * User-level device driver visible types
 */
typedef char            xwidgetnum_t;	/* xtalk widget number  (0..15) */

#define XWIDGET_NONE		(-1)

typedef int xwidget_part_num_t;	/* xtalk widget part number */

#define XWIDGET_PART_NUM_NONE	(-1)

typedef int             xwidget_rev_num_t;	/* xtalk widget revision number */

#define XWIDGET_REV_NUM_NONE	(-1)

typedef int xwidget_mfg_num_t;	/* xtalk widget manufacturing ID */

#define XWIDGET_MFG_NUM_NONE	(-1)

typedef struct xtalk_piomap_s *xtalk_piomap_t;

/* It is often convenient to fold the XIO target port
 * number into the XIO address.
 */
#define	XIO_NOWHERE	(0xFFFFFFFFFFFFFFFFull)
#define	XIO_ADDR_BITS	(0x0000FFFFFFFFFFFFull)
#define	XIO_PORT_BITS	(0xF000000000000000ull)
#define	XIO_PORT_SHIFT	(60)

#define	XIO_PACKED(x)	(((x)&XIO_PORT_BITS) != 0)
#define	XIO_ADDR(x)	((x)&XIO_ADDR_BITS)
#define	XIO_PORT(x)	((xwidgetnum_t)(((x)&XIO_PORT_BITS) >> XIO_PORT_SHIFT))
#define	XIO_PACK(p,o)	((((uint64_t)(p))<<XIO_PORT_SHIFT) | ((o)&XIO_ADDR_BITS))


/*
 * Kernel/driver only definitions
 */
#if __KERNEL__

#include <asm/types.h>
#include <asm/sn/types.h>
#include <asm/sn/alenlist.h>
#include <asm/sn/ioerror.h>
#include <asm/sn/driver.h>
#include <asm/sn/dmamap.h>

struct xwidget_hwid_s;

/*
 *    Acceptable flag bits for xtalk service calls
 *
 * XTALK_FIXED: require that mappings be established
 *	using fixed sharable resources; address
 *	translation results will be permanently
 *	available. (PIOMAP_FIXED and DMAMAP_FIXED are
 *	the same numeric value and are acceptable).
 * XTALK_NOSLEEP: if any part of the operation would
 *	sleep waiting for resoruces, return an error
 *	instead. (PIOMAP_NOSLEEP and DMAMAP_NOSLEEP are
 *	the same numeric value and are acceptable).
 * XTALK_INPLACE: when operating on alenlist structures,
 *	reuse the source alenlist rather than creating a
 *	new one. (PIOMAP_INPLACE and DMAMAP_INPLACE are
 *	the same numeric value and are acceptable).
 */
#define	XTALK_FIXED		DMAMAP_FIXED
#define	XTALK_NOSLEEP		DMAMAP_NOSLEEP
#define	XTALK_INPLACE		DMAMAP_INPLACE

/* PIO MANAGEMENT */
typedef xtalk_piomap_t
xtalk_piomap_alloc_f    (vertex_hdl_t dev,	/* set up mapping for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 iopaddr_t xtalk_addr,	/* map for this xtalk_addr range */
			 size_t byte_count,
			 size_t byte_count_max,		/* maximum size of a mapping */
			 unsigned flags);	/* defined in sys/pio.h */
typedef void
xtalk_piomap_free_f     (xtalk_piomap_t xtalk_piomap);

typedef caddr_t
xtalk_piomap_addr_f     (xtalk_piomap_t xtalk_piomap,	/* mapping resources */
			 iopaddr_t xtalk_addr,	/* map for this xtalk address */
			 size_t byte_count);	/* map this many bytes */

typedef void
xtalk_piomap_done_f     (xtalk_piomap_t xtalk_piomap);

typedef caddr_t
xtalk_piotrans_addr_f   (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 iopaddr_t xtalk_addr,	/* Crosstalk address */
			 size_t byte_count,	/* map this many bytes */
			 unsigned flags);	/* (currently unused) */

extern caddr_t
xtalk_pio_addr		(vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 iopaddr_t xtalk_addr,	/* Crosstalk address */
			 size_t byte_count,	/* map this many bytes */
			 xtalk_piomap_t *xtalk_piomapp,	/* RETURNS mapping resources */
			 unsigned flags);	/* (currently unused) */

/* DMA MANAGEMENT */

typedef struct xtalk_dmamap_s *xtalk_dmamap_t;

typedef xtalk_dmamap_t
xtalk_dmamap_alloc_f    (vertex_hdl_t dev,	/* set up mappings for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 size_t byte_count_max,		/* max size of a mapping */
			 unsigned flags);	/* defined in dma.h */

typedef void
xtalk_dmamap_free_f     (xtalk_dmamap_t dmamap);

typedef iopaddr_t
xtalk_dmamap_addr_f     (xtalk_dmamap_t dmamap,		/* use these mapping resources */
			 paddr_t paddr,		/* map for this address */
			 size_t byte_count);	/* map this many bytes */

typedef alenlist_t
xtalk_dmamap_list_f     (xtalk_dmamap_t dmamap,		/* use these mapping resources */
			 alenlist_t alenlist,	/* map this address/length list */
			 unsigned flags);

typedef void
xtalk_dmamap_done_f     (xtalk_dmamap_t dmamap);

typedef iopaddr_t
xtalk_dmatrans_addr_f   (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 paddr_t paddr,		/* system physical address */
			 size_t byte_count,	/* length */
			 unsigned flags);

typedef alenlist_t
xtalk_dmatrans_list_f   (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 alenlist_t palenlist,	/* system address/length list */
			 unsigned flags);

typedef void
xtalk_dmamap_drain_f	(xtalk_dmamap_t map);	/* drain this map's channel */

typedef void
xtalk_dmaaddr_drain_f	(vertex_hdl_t vhdl,	/* drain channel from this device */
			 paddr_t addr,		/* to this physical address */
			 size_t bytes);		/* for this many bytes */

typedef void
xtalk_dmalist_drain_f	(vertex_hdl_t vhdl,	/* drain channel from this device */
			 alenlist_t list);	/* for this set of physical blocks */


/* INTERRUPT MANAGEMENT */

/*
 * A xtalk interrupt resource handle.  When resources are allocated
 * in order to satisfy a xtalk_intr_alloc request, a xtalk_intr handle
 * is returned.  xtalk_intr_connect associates a software handler with

 * these system resources.
 */
typedef struct xtalk_intr_s *xtalk_intr_t;


/*
 * When a crosstalk device connects an interrupt, it passes in a function
 * that knows how to set its xtalk interrupt register appropriately.  The
 * low-level interrupt code may invoke this function later in order to
 * migrate an interrupt transparently to the device driver(s) that use this
 * interrupt.
 *
 * The argument passed to this function contains enough information for a
 * crosstalk device to (re-)target an interrupt.  A function of this type
 * must be supplied by every crosstalk driver.
 */
typedef int
xtalk_intr_setfunc_f    (xtalk_intr_t intr_hdl);	/* interrupt handle */

typedef xtalk_intr_t
xtalk_intr_alloc_f      (vertex_hdl_t dev,	/* which crosstalk device */
			 device_desc_t dev_desc,	/* device descriptor */
			 vertex_hdl_t owner_dev);	/* owner of this intr */

typedef void
xtalk_intr_free_f       (xtalk_intr_t intr_hdl);

typedef int
xtalk_intr_connect_f    (xtalk_intr_t intr_hdl,		/* xtalk intr resource handle */
			intr_func_t intr_func,         /* xtalk intr handler */
			void *intr_arg,	/* arg to intr handler */
			xtalk_intr_setfunc_f *setfunc,		/* func to set intr hw */
			void *setfunc_arg);	/* arg to setfunc */

typedef void
xtalk_intr_disconnect_f (xtalk_intr_t intr_hdl);

typedef vertex_hdl_t
xtalk_intr_cpu_get_f    (xtalk_intr_t intr_hdl);	/* xtalk intr resource handle */

/* CONFIGURATION MANAGEMENT */

typedef void
xtalk_provider_startup_f (vertex_hdl_t xtalk_provider);

typedef void
xtalk_provider_shutdown_f (vertex_hdl_t xtalk_provider);

typedef void
xtalk_widgetdev_enable_f (vertex_hdl_t, int);

typedef void
xtalk_widgetdev_shutdown_f (vertex_hdl_t, int);

typedef int
xtalk_dma_enabled_f (vertex_hdl_t);

/* Error Management */

typedef int
xtalk_error_devenable_f (vertex_hdl_t xconn_vhdl,
			 int devnum,
			 int error_code);

/* Early Action Support */
typedef caddr_t
xtalk_early_piotrans_addr_f (xwidget_part_num_t part_num,
			     xwidget_mfg_num_t mfg_num,
			     int which,
			     iopaddr_t xtalk_addr,
			     size_t byte_count,
			     unsigned flags);

/*
 * Adapters that provide a crosstalk interface adhere to this software interface.
 */
typedef struct xtalk_provider_s {
    /* PIO MANAGEMENT */
    xtalk_piomap_alloc_f   *piomap_alloc;
    xtalk_piomap_free_f    *piomap_free;
    xtalk_piomap_addr_f    *piomap_addr;
    xtalk_piomap_done_f    *piomap_done;
    xtalk_piotrans_addr_f  *piotrans_addr;

    /* DMA MANAGEMENT */
    xtalk_dmamap_alloc_f   *dmamap_alloc;
    xtalk_dmamap_free_f    *dmamap_free;
    xtalk_dmamap_addr_f    *dmamap_addr;
    xtalk_dmamap_list_f    *dmamap_list;
    xtalk_dmamap_done_f    *dmamap_done;
    xtalk_dmatrans_addr_f  *dmatrans_addr;
    xtalk_dmatrans_list_f  *dmatrans_list;
    xtalk_dmamap_drain_f   *dmamap_drain;
    xtalk_dmaaddr_drain_f  *dmaaddr_drain;
    xtalk_dmalist_drain_f  *dmalist_drain;

    /* INTERRUPT MANAGEMENT */
    xtalk_intr_alloc_f     *intr_alloc;
    xtalk_intr_alloc_f     *intr_alloc_nothd;
    xtalk_intr_free_f      *intr_free;
    xtalk_intr_connect_f   *intr_connect;
    xtalk_intr_disconnect_f *intr_disconnect;

    /* CONFIGURATION MANAGEMENT */
    xtalk_provider_startup_f *provider_startup;
    xtalk_provider_shutdown_f *provider_shutdown;

    /* Error Management     */
    xtalk_error_devenable_f *error_devenable;
} xtalk_provider_t;

/* Crosstalk devices use these standard Crosstalk provider interfaces */
extern xtalk_piomap_alloc_f xtalk_piomap_alloc;
extern xtalk_piomap_free_f xtalk_piomap_free;
extern xtalk_piomap_addr_f xtalk_piomap_addr;
extern xtalk_piomap_done_f xtalk_piomap_done;
extern xtalk_piotrans_addr_f xtalk_piotrans_addr;
extern xtalk_dmamap_alloc_f xtalk_dmamap_alloc;
extern xtalk_dmamap_free_f xtalk_dmamap_free;
extern xtalk_dmamap_addr_f xtalk_dmamap_addr;
extern xtalk_dmamap_list_f xtalk_dmamap_list;
extern xtalk_dmamap_done_f xtalk_dmamap_done;
extern xtalk_dmatrans_addr_f xtalk_dmatrans_addr;
extern xtalk_dmatrans_list_f xtalk_dmatrans_list;
extern xtalk_dmamap_drain_f xtalk_dmamap_drain;
extern xtalk_dmaaddr_drain_f xtalk_dmaaddr_drain;
extern xtalk_dmalist_drain_f xtalk_dmalist_drain;
extern xtalk_intr_alloc_f xtalk_intr_alloc;
extern xtalk_intr_alloc_f xtalk_intr_alloc_nothd;
extern xtalk_intr_free_f xtalk_intr_free;
extern xtalk_intr_connect_f xtalk_intr_connect;
extern xtalk_intr_disconnect_f xtalk_intr_disconnect;
extern xtalk_intr_cpu_get_f xtalk_intr_cpu_get;
extern xtalk_provider_startup_f xtalk_provider_startup;
extern xtalk_provider_shutdown_f xtalk_provider_shutdown;
extern xtalk_widgetdev_enable_f xtalk_widgetdev_enable;
extern xtalk_widgetdev_shutdown_f xtalk_widgetdev_shutdown;
extern xtalk_dma_enabled_f xtalk_dma_enabled;
extern xtalk_error_devenable_f xtalk_error_devenable;
extern xtalk_early_piotrans_addr_f xtalk_early_piotrans_addr;

/* error management */

extern int              xtalk_error_handler(vertex_hdl_t,
					    int,
					    ioerror_mode_t,
					    ioerror_t *);

/*
 * Generic crosstalk interface, for use with all crosstalk providers
 * and all crosstalk devices.
 */
typedef unchar xtalk_intr_vector_t;	/* crosstalk interrupt vector (0..255) */

#define XTALK_INTR_VECTOR_NONE	(xtalk_intr_vector_t)0

/* Generic crosstalk interrupt interfaces */
extern vertex_hdl_t     xtalk_intr_dev_get(xtalk_intr_t xtalk_intr);
extern xwidgetnum_t     xtalk_intr_target_get(xtalk_intr_t xtalk_intr);
extern xtalk_intr_vector_t xtalk_intr_vector_get(xtalk_intr_t xtalk_intr);
extern iopaddr_t        xtalk_intr_addr_get(xtalk_intr_t xtalk_intr);
extern vertex_hdl_t     xtalk_intr_cpu_get(xtalk_intr_t xtalk_intr);
extern void            *xtalk_intr_sfarg_get(xtalk_intr_t xtalk_intr);

/* Generic crosstalk pio interfaces */
extern vertex_hdl_t     xtalk_pio_dev_get(xtalk_piomap_t xtalk_piomap);
extern xwidgetnum_t     xtalk_pio_target_get(xtalk_piomap_t xtalk_piomap);
extern iopaddr_t        xtalk_pio_xtalk_addr_get(xtalk_piomap_t xtalk_piomap);
extern size_t           xtalk_pio_mapsz_get(xtalk_piomap_t xtalk_piomap);
extern caddr_t          xtalk_pio_kvaddr_get(xtalk_piomap_t xtalk_piomap);

/* Generic crosstalk dma interfaces */
extern vertex_hdl_t     xtalk_dma_dev_get(xtalk_dmamap_t xtalk_dmamap);
extern xwidgetnum_t     xtalk_dma_target_get(xtalk_dmamap_t xtalk_dmamap);

/* Register/unregister Crosstalk providers and get implementation handle */
extern void             xtalk_set_early_piotrans_addr(xtalk_early_piotrans_addr_f *);
extern void             xtalk_provider_register(vertex_hdl_t provider, xtalk_provider_t *xtalk_fns);
extern void             xtalk_provider_unregister(vertex_hdl_t provider);
extern xtalk_provider_t *xtalk_provider_fns_get(vertex_hdl_t provider);

/* Crosstalk Switch generic layer, for use by initialization code */
extern void             xswitch_census(vertex_hdl_t xswitchv);
extern void             xswitch_init_widgets(vertex_hdl_t xswitchv);

/* early init interrupt management */

typedef void
xwidget_intr_preset_f   (void *which_widget,
			 int which_widget_intr,
			 xwidgetnum_t targ,
			 iopaddr_t addr,
			 xtalk_intr_vector_t vect);

typedef void
xtalk_intr_prealloc_f   (void *which_xtalk,
			 xtalk_intr_vector_t xtalk_vector,
			 xwidget_intr_preset_f *preset_func,
			 void *which_widget,
			 int which_widget_intr);

typedef void
xtalk_intr_preconn_f    (void *which_xtalk,
			 xtalk_intr_vector_t xtalk_vector,
			 intr_func_t intr_func,
			 intr_arg_t intr_arg);


#define XTALK_ADDR_TO_UPPER(xtalk_addr) (((iopaddr_t)(xtalk_addr) >> 32) & 0xffff)
#define XTALK_ADDR_TO_LOWER(xtalk_addr) ((iopaddr_t)(xtalk_addr) & 0xffffffff)

typedef xtalk_intr_setfunc_f *xtalk_intr_setfunc_t;

typedef void		xtalk_iter_f(vertex_hdl_t vhdl);

extern void		xtalk_iterate(char *prefix, xtalk_iter_f *func);

#endif				/* __KERNEL__ */
#endif				/* _ASM_SN_XTALK_XTALK_H */
