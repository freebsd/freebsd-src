/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <asm/sn/sgi.h>
#include <asm/sn/driver.h>
#include <asm/sn/io.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/xtalk/xswitch.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/xtalk/xtalk_private.h>

/*
 * Implement crosstalk provider operations.  The xtalk* layer provides a
 * platform-independent interface for crosstalk devices.  This layer
 * switches among the possible implementations of a crosstalk adapter.
 *
 * On platforms with only one possible xtalk provider, macros can be
 * set up at the top that cause the table lookups and indirections to
 * completely disappear.
 */

#define	NEW(ptr)	(ptr = kmalloc(sizeof (*(ptr)), GFP_KERNEL))
#define	DEL(ptr)	(kfree(ptr))

char                    widget_info_fingerprint[] = "widget_info";

#define	DEV_FUNC(dev,func)	hub_##func
#define	CAST_PIOMAP(x)		((hub_piomap_t)(x))
#define	CAST_DMAMAP(x)		((hub_dmamap_t)(x))
#define	CAST_INTR(x)		((hub_intr_t)(x))

/* =====================================================================
 *            Function Table of Contents
 */
xtalk_piomap_t          xtalk_piomap_alloc(vertex_hdl_t, device_desc_t, iopaddr_t, size_t, size_t, unsigned);
void                    xtalk_piomap_free(xtalk_piomap_t);
caddr_t                 xtalk_piomap_addr(xtalk_piomap_t, iopaddr_t, size_t);
void                    xtalk_piomap_done(xtalk_piomap_t);
caddr_t                 xtalk_piotrans_addr(vertex_hdl_t, device_desc_t, iopaddr_t, size_t, unsigned);
caddr_t                 xtalk_pio_addr(vertex_hdl_t, device_desc_t, iopaddr_t, size_t, xtalk_piomap_t *, unsigned);
void                    xtalk_set_early_piotrans_addr(xtalk_early_piotrans_addr_f *);
caddr_t                 xtalk_early_piotrans_addr(xwidget_part_num_t, xwidget_mfg_num_t, int, iopaddr_t, size_t, unsigned);
static caddr_t          null_xtalk_early_piotrans_addr(xwidget_part_num_t, xwidget_mfg_num_t, int, iopaddr_t, size_t, unsigned);
xtalk_dmamap_t          xtalk_dmamap_alloc(vertex_hdl_t, device_desc_t, size_t, unsigned);
void                    xtalk_dmamap_free(xtalk_dmamap_t);
iopaddr_t               xtalk_dmamap_addr(xtalk_dmamap_t, paddr_t, size_t);
alenlist_t              xtalk_dmamap_list(xtalk_dmamap_t, alenlist_t, unsigned);
void                    xtalk_dmamap_done(xtalk_dmamap_t);
iopaddr_t               xtalk_dmatrans_addr(vertex_hdl_t, device_desc_t, paddr_t, size_t, unsigned);
alenlist_t              xtalk_dmatrans_list(vertex_hdl_t, device_desc_t, alenlist_t, unsigned);
void			xtalk_dmamap_drain(xtalk_dmamap_t);
void			xtalk_dmaaddr_drain(vertex_hdl_t, iopaddr_t, size_t);
void			xtalk_dmalist_drain(vertex_hdl_t, alenlist_t);
xtalk_intr_t            xtalk_intr_alloc(vertex_hdl_t, device_desc_t, vertex_hdl_t);
xtalk_intr_t            xtalk_intr_alloc_nothd(vertex_hdl_t, device_desc_t, vertex_hdl_t);
void                    xtalk_intr_free(xtalk_intr_t);
int                     xtalk_intr_connect(xtalk_intr_t, intr_func_t, intr_arg_t, xtalk_intr_setfunc_t, void *);
void                    xtalk_intr_disconnect(xtalk_intr_t);
vertex_hdl_t            xtalk_intr_cpu_get(xtalk_intr_t);
int                     xtalk_error_handler(vertex_hdl_t, int, ioerror_mode_t, ioerror_t *);
int                     xtalk_error_devenable(vertex_hdl_t, int, int);
void                    xtalk_provider_startup(vertex_hdl_t);
void                    xtalk_provider_shutdown(vertex_hdl_t);
vertex_hdl_t            xtalk_intr_dev_get(xtalk_intr_t);
xwidgetnum_t            xtalk_intr_target_get(xtalk_intr_t);
xtalk_intr_vector_t     xtalk_intr_vector_get(xtalk_intr_t);
iopaddr_t               xtalk_intr_addr_get(struct xtalk_intr_s *);
void                   *xtalk_intr_sfarg_get(xtalk_intr_t);
vertex_hdl_t            xtalk_pio_dev_get(xtalk_piomap_t);
xwidgetnum_t            xtalk_pio_target_get(xtalk_piomap_t);
iopaddr_t               xtalk_pio_xtalk_addr_get(xtalk_piomap_t);
ulong                   xtalk_pio_mapsz_get(xtalk_piomap_t);
caddr_t                 xtalk_pio_kvaddr_get(xtalk_piomap_t);
vertex_hdl_t            xtalk_dma_dev_get(xtalk_dmamap_t);
xwidgetnum_t            xtalk_dma_target_get(xtalk_dmamap_t);
xwidget_info_t          xwidget_info_chk(vertex_hdl_t);
xwidget_info_t          xwidget_info_get(vertex_hdl_t);
void                    xwidget_info_set(vertex_hdl_t, xwidget_info_t);
vertex_hdl_t            xwidget_info_dev_get(xwidget_info_t);
xwidgetnum_t            xwidget_info_id_get(xwidget_info_t);
vertex_hdl_t            xwidget_info_master_get(xwidget_info_t);
xwidgetnum_t            xwidget_info_masterid_get(xwidget_info_t);
xwidget_part_num_t      xwidget_info_part_num_get(xwidget_info_t);
xwidget_mfg_num_t       xwidget_info_mfg_num_get(xwidget_info_t);
char 			*xwidget_info_name_get(xwidget_info_t);
void                    xtalk_provider_register(vertex_hdl_t, xtalk_provider_t *);
void                    xtalk_provider_unregister(vertex_hdl_t);
xtalk_provider_t       *xtalk_provider_fns_get(vertex_hdl_t);
int                     xwidget_driver_register(xwidget_part_num_t, 
						xwidget_mfg_num_t, 
						char *, unsigned);
void                    xwidget_driver_unregister(char *);
int                     xwidget_register(xwidget_hwid_t, vertex_hdl_t, 
					 xwidgetnum_t, vertex_hdl_t, 
					 xwidgetnum_t);
int			xwidget_unregister(vertex_hdl_t);
void                    xwidget_reset(vertex_hdl_t);
char			*xwidget_name_get(vertex_hdl_t);
#if !defined(DEV_FUNC)
/*
 * There is more than one possible provider
 * for this platform. We need to examine the
 * master vertex of the current vertex for
 * a provider function structure, and indirect
 * through the appropriately named member.
 */
#define	DEV_FUNC(dev,func)	xwidget_to_provider_fns(dev)->func
#define	CAST_PIOMAP(x)		((xtalk_piomap_t)(x))
#define	CAST_DMAMAP(x)		((xtalk_dmamap_t)(x))
#define	CAST_INTR(x)		((xtalk_intr_t)(x))

static xtalk_provider_t *
xwidget_to_provider_fns(vertex_hdl_t xconn)
{
    xwidget_info_t          widget_info;
    xtalk_provider_t       *provider_fns;

    widget_info = xwidget_info_get(xconn);
    ASSERT(widget_info != NULL);

    provider_fns = xwidget_info_pops_get(widget_info);
    ASSERT(provider_fns != NULL);

    return (provider_fns);
}
#endif

/*
 * Many functions are not passed their vertex
 * information directly; rather, they must
 * dive through a resource map. These macros
 * are available to coordinate this detail.
 */
#define	PIOMAP_FUNC(map,func)	DEV_FUNC(map->xp_dev,func)
#define	DMAMAP_FUNC(map,func)	DEV_FUNC(map->xd_dev,func)
#define	INTR_FUNC(intr,func)	DEV_FUNC(intr_hdl->xi_dev,func)

/* =====================================================================
 *                    PIO MANAGEMENT
 *
 *      For mapping system virtual address space to
 *      xtalk space on a specified widget
 */

xtalk_piomap_t
xtalk_piomap_alloc(vertex_hdl_t dev,	/* set up mapping for this device */
		   device_desc_t dev_desc,	/* device descriptor */
		   iopaddr_t xtalk_addr,	/* map for this xtalk_addr range */
		   size_t byte_count,
		   size_t byte_count_max,	/* maximum size of a mapping */
		   unsigned flags)
{				/* defined in sys/pio.h */
    return (xtalk_piomap_t) DEV_FUNC(dev, piomap_alloc)
	(dev, dev_desc, xtalk_addr, byte_count, byte_count_max, flags);
}


void
xtalk_piomap_free(xtalk_piomap_t xtalk_piomap)
{
    PIOMAP_FUNC(xtalk_piomap, piomap_free)
	(CAST_PIOMAP(xtalk_piomap));
}


caddr_t
xtalk_piomap_addr(xtalk_piomap_t xtalk_piomap,	/* mapping resources */
		  iopaddr_t xtalk_addr,		/* map for this xtalk address */
		  size_t byte_count)
{				/* map this many bytes */
    return PIOMAP_FUNC(xtalk_piomap, piomap_addr)
	(CAST_PIOMAP(xtalk_piomap), xtalk_addr, byte_count);
}


void
xtalk_piomap_done(xtalk_piomap_t xtalk_piomap)
{
    PIOMAP_FUNC(xtalk_piomap, piomap_done)
	(CAST_PIOMAP(xtalk_piomap));
}


caddr_t
xtalk_piotrans_addr(vertex_hdl_t dev,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    iopaddr_t xtalk_addr,	/* Crosstalk address */
		    size_t byte_count,	/* map this many bytes */
		    unsigned flags)
{				/* (currently unused) */
    return DEV_FUNC(dev, piotrans_addr)
	(dev, dev_desc, xtalk_addr, byte_count, flags);
}

caddr_t
xtalk_pio_addr(vertex_hdl_t dev,	/* translate for this device */
	       device_desc_t dev_desc,	/* device descriptor */
	       iopaddr_t addr,		/* starting address (or offset in window) */
	       size_t byte_count,	/* map this many bytes */
	       xtalk_piomap_t *mapp,	/* where to return the map pointer */
	       unsigned flags)
{					/* PIO flags */
    xtalk_piomap_t          map = 0;
    caddr_t                 res;

    if (mapp)
	*mapp = 0;			/* record "no map used" */

    res = xtalk_piotrans_addr
	(dev, dev_desc, addr, byte_count, flags);
    if (res)
	return res;			/* xtalk_piotrans worked */

    map = xtalk_piomap_alloc
	(dev, dev_desc, addr, byte_count, byte_count, flags);
    if (!map)
	return res;			/* xtalk_piomap_alloc failed */

    res = xtalk_piomap_addr
	(map, addr, byte_count);
    if (!res) {
	xtalk_piomap_free(map);
	return res;			/* xtalk_piomap_addr failed */
    }
    if (mapp)
	*mapp = map;			/* pass back map used */

    return res;				/* xtalk_piomap_addr succeeded */
}

/* =====================================================================
 *            EARLY PIOTRANS SUPPORT
 *
 *      There are places where drivers (mgras, for instance)
 *      need to get PIO translations before the infrastructure
 *      is extended to them (setting up textports, for
 *      instance). These drivers should call
 *      xtalk_early_piotrans_addr with their xtalk ID
 *      information, a sequence number (so we can use the second
 *      mgras for instance), and the usual piotrans parameters.
 *
 *      Machine specific code should provide an implementation
 *      of early_piotrans_addr, and present a pointer to this
 *      function to xtalk_set_early_piotrans_addr so it can be
 *      used by clients without the clients having to know what
 *      platform or what xtalk provider is in use.
 */

static xtalk_early_piotrans_addr_f null_xtalk_early_piotrans_addr;

xtalk_early_piotrans_addr_f *impl_early_piotrans_addr = null_xtalk_early_piotrans_addr;

/* xtalk_set_early_piotrans_addr:
 * specify the early_piotrans_addr implementation function.
 */
void
xtalk_set_early_piotrans_addr(xtalk_early_piotrans_addr_f *impl)
{
    impl_early_piotrans_addr = impl;
}

/* xtalk_early_piotrans_addr:
 * figure out a PIO address for the "nth" crosstalk widget that
 * matches the specified part and mfgr number. Returns NULL if
 * there is no such widget, or if the requested mapping can not
 * be constructed.
 * Limitations on which crosstalk slots (and busses) are
 * checked, and definitions of the ordering of the search across
 * the crosstalk slots, are defined by the platform.
 */
caddr_t
xtalk_early_piotrans_addr(xwidget_part_num_t part_num,
			  xwidget_mfg_num_t mfg_num,
			  int which,
			  iopaddr_t xtalk_addr,
			  size_t byte_count,
			  unsigned flags)
{
    return impl_early_piotrans_addr
	(part_num, mfg_num, which, xtalk_addr, byte_count, flags);
}

/* null_xtalk_early_piotrans_addr:
 * used as the early_piotrans_addr implementation until and
 * unless a real implementation is provided. In DEBUG kernels,
 * we want to know who is calling before the implementation is
 * registered; in non-DEBUG kernels, return NULL representing
 * lack of mapping support.
 */
/*ARGSUSED */
static caddr_t
null_xtalk_early_piotrans_addr(xwidget_part_num_t part_num,
			       xwidget_mfg_num_t mfg_num,
			       int which,
			       iopaddr_t xtalk_addr,
			       size_t byte_count,
			       unsigned flags)
{
#if DEBUG
    panic("null_xtalk_early_piotrans_addr");
#endif
    return NULL;
}

/* =====================================================================
 *                    DMA MANAGEMENT
 *
 *      For mapping from crosstalk space to system
 *      physical space.
 */

xtalk_dmamap_t
xtalk_dmamap_alloc(vertex_hdl_t dev,	/* set up mappings for this device */
		   device_desc_t dev_desc,	/* device descriptor */
		   size_t byte_count_max,	/* max size of a mapping */
		   unsigned flags)
{				/* defined in dma.h */
    return (xtalk_dmamap_t) DEV_FUNC(dev, dmamap_alloc)
	(dev, dev_desc, byte_count_max, flags);
}


void
xtalk_dmamap_free(xtalk_dmamap_t xtalk_dmamap)
{
    DMAMAP_FUNC(xtalk_dmamap, dmamap_free)
	(CAST_DMAMAP(xtalk_dmamap));
}


iopaddr_t
xtalk_dmamap_addr(xtalk_dmamap_t xtalk_dmamap,	/* use these mapping resources */
		  paddr_t paddr,	/* map for this address */
		  size_t byte_count)
{				/* map this many bytes */
    return DMAMAP_FUNC(xtalk_dmamap, dmamap_addr)
	(CAST_DMAMAP(xtalk_dmamap), paddr, byte_count);
}


alenlist_t
xtalk_dmamap_list(xtalk_dmamap_t xtalk_dmamap,	/* use these mapping resources */
		  alenlist_t alenlist,	/* map this Address/Length List */
		  unsigned flags)
{
    return DMAMAP_FUNC(xtalk_dmamap, dmamap_list)
	(CAST_DMAMAP(xtalk_dmamap), alenlist, flags);
}


void
xtalk_dmamap_done(xtalk_dmamap_t xtalk_dmamap)
{
    DMAMAP_FUNC(xtalk_dmamap, dmamap_done)
	(CAST_DMAMAP(xtalk_dmamap));
}


iopaddr_t
xtalk_dmatrans_addr(vertex_hdl_t dev,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    paddr_t paddr,	/* system physical address */
		    size_t byte_count,	/* length */
		    unsigned flags)
{				/* defined in dma.h */
    return DEV_FUNC(dev, dmatrans_addr)
	(dev, dev_desc, paddr, byte_count, flags);
}


alenlist_t
xtalk_dmatrans_list(vertex_hdl_t dev,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    alenlist_t palenlist,	/* system address/length list */
		    unsigned flags)
{				/* defined in dma.h */
    return DEV_FUNC(dev, dmatrans_list)
	(dev, dev_desc, palenlist, flags);
}

void
xtalk_dmamap_drain(xtalk_dmamap_t map)
{
    DMAMAP_FUNC(map, dmamap_drain)
	(CAST_DMAMAP(map));
}

void
xtalk_dmaaddr_drain(vertex_hdl_t dev, paddr_t addr, size_t size)
{
    DEV_FUNC(dev, dmaaddr_drain)
	(dev, addr, size);
}

void
xtalk_dmalist_drain(vertex_hdl_t dev, alenlist_t list)
{
    DEV_FUNC(dev, dmalist_drain)
	(dev, list);
}

/* =====================================================================
 *                    INTERRUPT MANAGEMENT
 *
 *      Allow crosstalk devices to establish interrupts
 */

/*
 * Allocate resources required for an interrupt as specified in intr_desc.
 * Return resource handle in intr_hdl.
 */
xtalk_intr_t
xtalk_intr_alloc(vertex_hdl_t dev,	/* which Crosstalk device */
		 device_desc_t dev_desc,	/* device descriptor */
		 vertex_hdl_t owner_dev)
{				/* owner of this interrupt */
    return (xtalk_intr_t) DEV_FUNC(dev, intr_alloc)
	(dev, dev_desc, owner_dev);
}

/*
 * Allocate resources required for an interrupt as specified in dev_desc.
 * Unconditionally setup resources to be non-threaded.
 * Return resource handle in intr_hdl.
 */
xtalk_intr_t
xtalk_intr_alloc_nothd(vertex_hdl_t dev,	/* which Crosstalk device */
		 	device_desc_t dev_desc,	/* device descriptor */
		 	vertex_hdl_t owner_dev)	/* owner of this interrupt */
{
    return (xtalk_intr_t) DEV_FUNC(dev, intr_alloc_nothd)
	(dev, dev_desc, owner_dev);
}

/*
 * Free resources consumed by intr_alloc.
 */
void
xtalk_intr_free(xtalk_intr_t intr_hdl)
{
    INTR_FUNC(intr_hdl, intr_free)
	(CAST_INTR(intr_hdl));
}


/*
 * Associate resources allocated with a previous xtalk_intr_alloc call with the
 * described handler, arg, name, etc.
 *
 * Returns 0 on success, returns <0 on failure.
 */
int
xtalk_intr_connect(xtalk_intr_t intr_hdl,	/* xtalk intr resource handle */
		   intr_func_t intr_func,       /* xtalk intr handler */
		   intr_arg_t intr_arg,         /* arg to intr handler */
		   xtalk_intr_setfunc_t setfunc,	/* func to set intr hw */
		   void *setfunc_arg)	/* arg to setfunc */
{
    return INTR_FUNC(intr_hdl, intr_connect)
	(CAST_INTR(intr_hdl), intr_func, intr_arg, setfunc, setfunc_arg);
}


/*
 * Disassociate handler with the specified interrupt.
 */
void
xtalk_intr_disconnect(xtalk_intr_t intr_hdl)
{
    INTR_FUNC(intr_hdl, intr_disconnect)
	(CAST_INTR(intr_hdl));
}


/*
 * Return a hwgraph vertex that represents the CPU currently
 * targeted by an interrupt.
 */
vertex_hdl_t
xtalk_intr_cpu_get(xtalk_intr_t intr_hdl)
{
      return (vertex_hdl_t)0;
}


/*
 * =====================================================================
 *                      ERROR MANAGEMENT
 */

/*
 * xtalk_error_handler:
 * pass this error on to the handler registered
 * at the specified xtalk connecdtion point,
 * or complain about it here if there is no handler.
 *
 * This routine plays two roles during error delivery
 * to most widgets: first, the external agent (heart,
 * hub, or whatever) calls in with the error and the
 * connect point representing the crosstalk switch,
 * or whatever crosstalk device is directly connected
 * to the agent.
 *
 * If there is a switch, it will generally look at the
 * widget number stashed in the ioerror structure; and,
 * if the error came from some widget other than the
 * switch, it will call back into xtalk_error_handler
 * with the connection point of the offending port.
 */
int
xtalk_error_handler(
		       vertex_hdl_t xconn,
		       int error_code,
		       ioerror_mode_t mode,
		       ioerror_t *ioerror)
{
    xwidget_info_t          xwidget_info;

    xwidget_info = xwidget_info_get(xconn);
    /* Make sure that xwidget_info is a valid pointer before derefencing it.
     * We could come in here during very early initialization. 
     */
    if (xwidget_info && xwidget_info->w_efunc)
	return xwidget_info->w_efunc
	    (xwidget_info->w_einfo,
	     error_code, mode, ioerror);
    /*
     * no error handler registered for
     * the offending port. it's not clear
     * what needs to be done, but reporting
     * it would be a good thing, unless it
     * is a mode that requires nothing.
     */
    if ((mode == MODE_DEVPROBE) || (mode == MODE_DEVUSERERROR) ||
	(mode == MODE_DEVREENABLE))
	return IOERROR_HANDLED;

#if defined(SUPPORT_PRINTING_V_FORMAT)
    printk(KERN_WARNING "Xbow at %v encountered Fatal error", xconn);
#else
    printk(KERN_WARNING "Xbow at 0x%p encountered Fatal error", (void *)xconn);
#endif
    snia_ioerror_dump("xtalk", error_code, mode, ioerror);

    return IOERROR_UNHANDLED;
}

int
xtalk_error_devenable(vertex_hdl_t xconn_vhdl, int devnum, int error_code)
{
    return DEV_FUNC(xconn_vhdl, error_devenable) (xconn_vhdl, devnum, error_code);
}


/* =====================================================================
 *                    CONFIGURATION MANAGEMENT
 */

/*
 * Startup a crosstalk provider
 */
void
xtalk_provider_startup(vertex_hdl_t xtalk_provider)
{
    DEV_FUNC(xtalk_provider, provider_startup)
	(xtalk_provider);
}


/*
 * Shutdown a crosstalk provider
 */
void
xtalk_provider_shutdown(vertex_hdl_t xtalk_provider)
{
    DEV_FUNC(xtalk_provider, provider_shutdown)
	(xtalk_provider);
}

/* 
 * Enable a device on a xtalk widget 
 */
void
xtalk_widgetdev_enable(vertex_hdl_t xconn_vhdl, int devnum)
{
	return;
}

/* 
 * Shutdown a device on a xtalk widget 
 */
void
xtalk_widgetdev_shutdown(vertex_hdl_t xconn_vhdl, int devnum)
{
	return;
}

int
xtalk_dma_enabled(vertex_hdl_t xconn_vhdl)
{
    return DEV_FUNC(xconn_vhdl, dma_enabled) (xconn_vhdl);
}
/*
 * Generic crosstalk functions, for use with all crosstalk providers
 * and all crosstalk devices.
 */

/****** Generic crosstalk interrupt interfaces ******/
vertex_hdl_t
xtalk_intr_dev_get(xtalk_intr_t xtalk_intr)
{
    return (xtalk_intr->xi_dev);
}

xwidgetnum_t
xtalk_intr_target_get(xtalk_intr_t xtalk_intr)
{
    return (xtalk_intr->xi_target);
}

xtalk_intr_vector_t
xtalk_intr_vector_get(xtalk_intr_t xtalk_intr)
{
    return (xtalk_intr->xi_vector);
}

iopaddr_t
xtalk_intr_addr_get(struct xtalk_intr_s *xtalk_intr)
{
    return (xtalk_intr->xi_addr);
}

void                   *
xtalk_intr_sfarg_get(xtalk_intr_t xtalk_intr)
{
    return (xtalk_intr->xi_sfarg);
}

/****** Generic crosstalk pio interfaces ******/
vertex_hdl_t
xtalk_pio_dev_get(xtalk_piomap_t xtalk_piomap)
{
    return (xtalk_piomap->xp_dev);
}

xwidgetnum_t
xtalk_pio_target_get(xtalk_piomap_t xtalk_piomap)
{
    return (xtalk_piomap->xp_target);
}

iopaddr_t
xtalk_pio_xtalk_addr_get(xtalk_piomap_t xtalk_piomap)
{
    return (xtalk_piomap->xp_xtalk_addr);
}

ulong
xtalk_pio_mapsz_get(xtalk_piomap_t xtalk_piomap)
{
    return (xtalk_piomap->xp_mapsz);
}

caddr_t
xtalk_pio_kvaddr_get(xtalk_piomap_t xtalk_piomap)
{
    return (xtalk_piomap->xp_kvaddr);
}


/****** Generic crosstalk dma interfaces ******/
vertex_hdl_t
xtalk_dma_dev_get(xtalk_dmamap_t xtalk_dmamap)
{
    return (xtalk_dmamap->xd_dev);
}

xwidgetnum_t
xtalk_dma_target_get(xtalk_dmamap_t xtalk_dmamap)
{
    return (xtalk_dmamap->xd_target);
}


/****** Generic crosstalk widget information interfaces ******/

/* xwidget_info_chk:
 * check to see if this vertex is a widget;
 * if so, return its widget_info (if any).
 * if not, return NULL.
 */
xwidget_info_t
xwidget_info_chk(vertex_hdl_t xwidget)
{
    arbitrary_info_t        ainfo = 0;

    hwgraph_info_get_LBL(xwidget, INFO_LBL_XWIDGET, &ainfo);
    return (xwidget_info_t) ainfo;
}


xwidget_info_t
xwidget_info_get(vertex_hdl_t xwidget)
{
    xwidget_info_t          widget_info;

    widget_info = (xwidget_info_t)
	hwgraph_fastinfo_get(xwidget);

    return (widget_info);
}

void
xwidget_info_set(vertex_hdl_t xwidget, xwidget_info_t widget_info)
{
    if (widget_info != NULL)
	widget_info->w_fingerprint = widget_info_fingerprint;

    hwgraph_fastinfo_set(xwidget, (arbitrary_info_t) widget_info);

    /* Also, mark this vertex as an xwidget,
     * and use the widget_info, so xwidget_info_chk
     * can work (and be fairly efficient).
     */
    hwgraph_info_add_LBL(xwidget, INFO_LBL_XWIDGET,
			 (arbitrary_info_t) widget_info);
}

vertex_hdl_t
xwidget_info_dev_get(xwidget_info_t xwidget_info)
{
    if (xwidget_info == NULL)
	panic("xwidget_info_dev_get: null xwidget_info");
    return (xwidget_info->w_vertex);
}

xwidgetnum_t
xwidget_info_id_get(xwidget_info_t xwidget_info)
{
    if (xwidget_info == NULL)
	panic("xwidget_info_id_get: null xwidget_info");
    return (xwidget_info->w_id);
}


vertex_hdl_t
xwidget_info_master_get(xwidget_info_t xwidget_info)
{
    if (xwidget_info == NULL)
	panic("xwidget_info_master_get: null xwidget_info");
    return (xwidget_info->w_master);
}

xwidgetnum_t
xwidget_info_masterid_get(xwidget_info_t xwidget_info)
{
    if (xwidget_info == NULL)
	panic("xwidget_info_masterid_get: null xwidget_info");
    return (xwidget_info->w_masterid);
}

xwidget_part_num_t
xwidget_info_part_num_get(xwidget_info_t xwidget_info)
{
    if (xwidget_info == NULL)
	panic("xwidget_info_part_num_get: null xwidget_info");
    return (xwidget_info->w_hwid.part_num);
}

xwidget_mfg_num_t
xwidget_info_mfg_num_get(xwidget_info_t xwidget_info)
{
    if (xwidget_info == NULL)
	panic("xwidget_info_mfg_num_get: null xwidget_info");
    return (xwidget_info->w_hwid.mfg_num);
}
/* Extract the widget name from the widget information
 * for the xtalk widget.
 */
char *
xwidget_info_name_get(xwidget_info_t xwidget_info)
{
    if (xwidget_info == NULL)
	panic("xwidget_info_name_get: null xwidget_info");
    return(xwidget_info->w_name);
}
/****** Generic crosstalk initialization interfaces ******/

/*
 * Associate a set of xtalk_provider functions with a vertex.
 */
void
xtalk_provider_register(vertex_hdl_t provider, xtalk_provider_t *xtalk_fns)
{
    hwgraph_fastinfo_set(provider, (arbitrary_info_t) xtalk_fns);
}

/*
 * Disassociate a set of xtalk_provider functions with a vertex.
 */
void
xtalk_provider_unregister(vertex_hdl_t provider)
{
    hwgraph_fastinfo_set(provider, (arbitrary_info_t)NULL);
}

/*
 * Obtain a pointer to the xtalk_provider functions for a specified Crosstalk
 * provider.
 */
xtalk_provider_t       *
xtalk_provider_fns_get(vertex_hdl_t provider)
{
    return ((xtalk_provider_t *) hwgraph_fastinfo_get(provider));
}

/*
 * Inform xtalk infrastructure that a driver is no longer available for
 * handling any widgets.
 */
void
xwidget_driver_unregister(char *driver_prefix)
{
	return;
}

/*
 * Call some function with each vertex that
 * might be one of this driver's attach points.
 */
void
xtalk_iterate(char *driver_prefix,
	      xtalk_iter_f *func)
{
}

/*
 * xwidget_register:
 *	Register a xtalk device (xwidget) by doing the following.
 *      -allocate and initialize xwidget_info data
 *      -allocate a hwgraph vertex with name based on widget number (id)
 *      -look up the widget's initialization function and call it,
 *      or remember the vertex for later initialization.
 *
 */
int
xwidget_register(xwidget_hwid_t hwid,		/* widget's hardware ID */
		 vertex_hdl_t 	widget,		/* widget to initialize */
		 xwidgetnum_t 	id,		/* widget's target id (0..f) */
		 vertex_hdl_t 	master,		/* widget's master vertex */
		 xwidgetnum_t 	targetid)	/* master's target id (9/a) */
{			
    xwidget_info_t          widget_info;
    char		    *s,devnm[MAXDEVNAME];

    /* Allocate widget_info and associate it with widget vertex */
    NEW(widget_info);

    /* Initialize widget_info */
    widget_info->w_vertex = widget;
    widget_info->w_id = id;
    widget_info->w_master = master;
    widget_info->w_masterid = targetid;
    widget_info->w_hwid = *hwid;	/* structure copy */
    widget_info->w_efunc = 0;
    widget_info->w_einfo = 0;
    /*
     * get the name of this xwidget vertex and keep the info.
     * This is needed during errors and interupts, but as
     * long as we have it, we can use it elsewhere.
     */
    s = dev_to_name(widget,devnm,MAXDEVNAME);
    widget_info->w_name = kmalloc(strlen(s) + 1, GFP_KERNEL);
    strcpy(widget_info->w_name,s);
    
    xwidget_info_set(widget, widget_info);

    device_master_set(widget, master);

    /* 
     * Add pointer to async attach info -- tear down will be done when
     * the particular descendant is done with the info.
     */
    return cdl_add_connpt(hwid->part_num, hwid->mfg_num,
                          widget, 0);
}

/*
 * xwidget_unregister :
 *	Unregister the xtalk device and detach all its hwgraph namespace.
 */
int
xwidget_unregister(vertex_hdl_t widget)
{
    xwidget_info_t	widget_info;
    xwidget_hwid_t	hwid;

    /* Make sure that we have valid widget information initialized */
    if (!(widget_info = xwidget_info_get(widget)))
	return(1);

    /* Remove the inventory information associated
     * with the widget.
     */
    hwgraph_inventory_remove(widget, -1, -1, -1, -1, -1);
    
    hwid = &(widget_info->w_hwid);

    /* Clean out the xwidget information */
    (void)kfree(widget_info->w_name);
    memset((void *)widget_info, 0, sizeof(widget_info));
    DEL(widget_info);
    
    return(0);
}

void
xwidget_error_register(vertex_hdl_t xwidget,
		       error_handler_f *efunc,
		       error_handler_arg_t einfo)
{
    xwidget_info_t          xwidget_info;

    xwidget_info = xwidget_info_get(xwidget);
    ASSERT(xwidget_info != NULL);
    xwidget_info->w_efunc = efunc;
    xwidget_info->w_einfo = einfo;
}

/*
 * Issue a link reset to a widget.
 */
void
xwidget_reset(vertex_hdl_t xwidget)
{
    xswitch_reset_link(xwidget);
}


void
xwidget_gfx_reset(vertex_hdl_t xwidget)
{
	return;
}

#define ANON_XWIDGET_NAME	"No Name"	/* Default Widget Name */

/* Get the canonical hwgraph  name of xtalk widget */
char *
xwidget_name_get(vertex_hdl_t xwidget_vhdl)
{
	xwidget_info_t  info;

	/* If we have a bogus widget handle then return
	 * a default anonymous widget name.
	 */
	if (xwidget_vhdl == GRAPH_VERTEX_NONE)
	    return(ANON_XWIDGET_NAME);
	/* Read the widget name stored in the widget info
	 * for the widget setup during widget initialization.
	 */
	info = xwidget_info_get(xwidget_vhdl);
	ASSERT(info != NULL);
	return(xwidget_info_name_get(info));
}
