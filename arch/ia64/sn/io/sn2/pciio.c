/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <asm/sn/sgi.h>
#include <asm/sn/xtalk/xbow.h>	/* Must be before iograph.h to get MAX_PORT_NUM */
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/ioerror_handling.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pciio_private.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/io.h>
#include <asm/sn/pci/pci_bus_cvlink.h>
#include <asm/sn/simulator.h>

#define DEBUG_PCIIO
#undef DEBUG_PCIIO	/* turn this on for yet more console output */


char                    pciio_info_fingerprint[] = "pciio_info";

/* =====================================================================
 *    PCI Generic Bus Provider
 * Implement PCI provider operations.  The pciio* layer provides a
 * platform-independent interface for PCI devices.  This layer
 * switches among the possible implementations of a PCI adapter.
 */

/* =====================================================================
 *    Provider Function Location SHORTCUT
 *
 * On platforms with only one possible PCI provider, macros can be
 * set up at the top that cause the table lookups and indirections to
 * completely disappear.
 */


/* =====================================================================
 *    Function Table of Contents
 */

#if !defined(DEV_FUNC)
extern pciio_provider_t *pciio_to_provider_fns(vertex_hdl_t dev);
#endif

/* =====================================================================
 *    Provider Function Location
 *
 *      If there is more than one possible provider for
 *      this platform, we need to examine the master
 *      vertex of the current vertex for a provider
 *      function structure, and indirect through the
 *      appropriately named member.
 */

#if !defined(DEV_FUNC)

pciio_provider_t *
pciio_to_provider_fns(vertex_hdl_t dev)
{
    pciio_info_t            card_info;
    pciio_provider_t       *provider_fns;

    /*
     * We're called with two types of vertices, one is
     * the bridge vertex (ends with "pci") and the other is the
     * pci slot vertex (ends with "pci/[0-8]").  For the first type
     * we need to get the provider from the PFUNCS label.  For
     * the second we get it from fastinfo/c_pops.
     */
    provider_fns = pciio_provider_fns_get(dev);
    if (provider_fns == NULL) {
	card_info = pciio_info_get(dev);
	if (card_info != NULL) {
		provider_fns = pciio_info_pops_get(card_info);
	}
    }

    if (provider_fns == NULL) {
	char devname[MAXDEVNAME];
	panic("%s: provider_fns == NULL", vertex_to_name(dev, devname, MAXDEVNAME));
    }
    return provider_fns;

}

#define DEV_FUNC(dev,func)	pciio_to_provider_fns(dev)->func
#define CAST_PIOMAP(x)		((pciio_piomap_t)(x))
#define CAST_DMAMAP(x)		((pciio_dmamap_t)(x))
#define CAST_INTR(x)		((pciio_intr_t)(x))
#endif

/*
 * Many functions are not passed their vertex
 * information directly; rather, they must
 * dive through a resource map. These macros
 * are available to coordinate this detail.
 */
#define PIOMAP_FUNC(map,func)		DEV_FUNC((map)->pp_dev,func)
#define DMAMAP_FUNC(map,func)		DEV_FUNC((map)->pd_dev,func)
#define INTR_FUNC(intr_hdl,func)	DEV_FUNC((intr_hdl)->pi_dev,func)

/* =====================================================================
 *          PIO MANAGEMENT
 *
 *      For mapping system virtual address space to
 *      pciio space on a specified card
 */

pciio_piomap_t
pciio_piomap_alloc(vertex_hdl_t dev,	/* set up mapping for this device */
		   device_desc_t dev_desc,	/* device descriptor */
		   pciio_space_t space,	/* CFG, MEM, IO, or a device-decoded window */
		   iopaddr_t addr,	/* lowest address (or offset in window) */
		   size_t byte_count,	/* size of region containing our mappings */
		   size_t byte_count_max,	/* maximum size of a mapping */
		   unsigned flags)
{					/* defined in sys/pio.h */
    return (pciio_piomap_t) DEV_FUNC(dev, piomap_alloc)
	(dev, dev_desc, space, addr, byte_count, byte_count_max, flags);
}

void
pciio_piomap_free(pciio_piomap_t pciio_piomap)
{
    PIOMAP_FUNC(pciio_piomap, piomap_free)
	(CAST_PIOMAP(pciio_piomap));
}

caddr_t
pciio_piomap_addr(pciio_piomap_t pciio_piomap,	/* mapping resources */
		  iopaddr_t pciio_addr,	/* map for this pciio address */
		  size_t byte_count)
{					/* map this many bytes */
    pciio_piomap->pp_kvaddr = PIOMAP_FUNC(pciio_piomap, piomap_addr)
	(CAST_PIOMAP(pciio_piomap), pciio_addr, byte_count);

    return pciio_piomap->pp_kvaddr;
}

void
pciio_piomap_done(pciio_piomap_t pciio_piomap)
{
    PIOMAP_FUNC(pciio_piomap, piomap_done)
	(CAST_PIOMAP(pciio_piomap));
}

caddr_t
pciio_piotrans_addr(vertex_hdl_t dev,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    pciio_space_t space,	/* CFG, MEM, IO, or a device-decoded window */
		    iopaddr_t addr,	/* starting address (or offset in window) */
		    size_t byte_count,	/* map this many bytes */
		    unsigned flags)
{					/* (currently unused) */
    return DEV_FUNC(dev, piotrans_addr)
	(dev, dev_desc, space, addr, byte_count, flags);
}

caddr_t
pciio_pio_addr(vertex_hdl_t dev,	/* translate for this device */
	       device_desc_t dev_desc,	/* device descriptor */
	       pciio_space_t space,	/* CFG, MEM, IO, or a device-decoded window */
	       iopaddr_t addr,		/* starting address (or offset in window) */
	       size_t byte_count,	/* map this many bytes */
	       pciio_piomap_t *mapp,	/* where to return the map pointer */
	       unsigned flags)
{					/* PIO flags */
    pciio_piomap_t          map = 0;
    int			    errfree = 0;
    caddr_t                 res;

    if (mapp) {
	map = *mapp;			/* possible pre-allocated map */
	*mapp = 0;			/* record "no map used" */
    }

    res = pciio_piotrans_addr
	(dev, dev_desc, space, addr, byte_count, flags);
    if (res)
	return res;			/* pciio_piotrans worked */

    if (!map) {
	map = pciio_piomap_alloc
	    (dev, dev_desc, space, addr, byte_count, byte_count, flags);
	if (!map)
	    return res;			/* pciio_piomap_alloc failed */
	errfree = 1;
    }

    res = pciio_piomap_addr
	(map, addr, byte_count);
    if (!res) {
	if (errfree)
	    pciio_piomap_free(map);
	return res;			/* pciio_piomap_addr failed */
    }
    if (mapp)
	*mapp = map;			/* pass back map used */

    return res;				/* pciio_piomap_addr succeeded */
}

iopaddr_t
pciio_piospace_alloc(vertex_hdl_t dev,	/* Device requiring space */
		     device_desc_t dev_desc,	/* Device descriptor */
		     pciio_space_t space,	/* MEM32/MEM64/IO */
		     size_t byte_count,	/* Size of mapping */
		     size_t align)
{					/* Alignment needed */
    if (align < PAGE_SIZE)
	align = PAGE_SIZE;
    return DEV_FUNC(dev, piospace_alloc)
	(dev, dev_desc, space, byte_count, align);
}

void
pciio_piospace_free(vertex_hdl_t dev,	/* Device freeing space */
		    pciio_space_t space,	/* Type of space        */
		    iopaddr_t pciaddr,	/* starting address */
		    size_t byte_count)
{					/* Range of address   */
    DEV_FUNC(dev, piospace_free)
	(dev, space, pciaddr, byte_count);
}

/* =====================================================================
 *          DMA MANAGEMENT
 *
 *      For mapping from pci space to system
 *      physical space.
 */

pciio_dmamap_t
pciio_dmamap_alloc(vertex_hdl_t dev,	/* set up mappings for this device */
		   device_desc_t dev_desc,	/* device descriptor */
		   size_t byte_count_max,	/* max size of a mapping */
		   unsigned flags)
{					/* defined in dma.h */
    return (pciio_dmamap_t) DEV_FUNC(dev, dmamap_alloc)
	(dev, dev_desc, byte_count_max, flags);
}

void
pciio_dmamap_free(pciio_dmamap_t pciio_dmamap)
{
    DMAMAP_FUNC(pciio_dmamap, dmamap_free)
	(CAST_DMAMAP(pciio_dmamap));
}

iopaddr_t
pciio_dmamap_addr(pciio_dmamap_t pciio_dmamap,	/* use these mapping resources */
		  paddr_t paddr,	/* map for this address */
		  size_t byte_count)
{					/* map this many bytes */
    return DMAMAP_FUNC(pciio_dmamap, dmamap_addr)
	(CAST_DMAMAP(pciio_dmamap), paddr, byte_count);
}

void
pciio_dmamap_done(pciio_dmamap_t pciio_dmamap)
{
    DMAMAP_FUNC(pciio_dmamap, dmamap_done)
	(CAST_DMAMAP(pciio_dmamap));
}

iopaddr_t
pciio_dmatrans_addr(vertex_hdl_t dev,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    paddr_t paddr,	/* system physical address */
		    size_t byte_count,	/* length */
		    unsigned flags)
{					/* defined in dma.h */
    return DEV_FUNC(dev, dmatrans_addr)
	(dev, dev_desc, paddr, byte_count, flags);
}

iopaddr_t
pciio_dma_addr(vertex_hdl_t dev,	/* translate for this device */
	       device_desc_t dev_desc,	/* device descriptor */
	       paddr_t paddr,		/* system physical address */
	       size_t byte_count,	/* length */
	       pciio_dmamap_t *mapp,	/* map to use, then map we used */
	       unsigned flags)
{					/* PIO flags */
    pciio_dmamap_t          map = 0;
    int			    errfree = 0;
    iopaddr_t               res;

    if (mapp) {
	map = *mapp;			/* possible pre-allocated map */
	*mapp = 0;			/* record "no map used" */
    }

    res = pciio_dmatrans_addr
	(dev, dev_desc, paddr, byte_count, flags);
    if (res)
	return res;			/* pciio_dmatrans worked */

    if (!map) {
	map = pciio_dmamap_alloc
	    (dev, dev_desc, byte_count, flags);
	if (!map)
	    return res;			/* pciio_dmamap_alloc failed */
	errfree = 1;
    }

    res = pciio_dmamap_addr
	(map, paddr, byte_count);
    if (!res) {
	if (errfree)
	    pciio_dmamap_free(map);
	return res;			/* pciio_dmamap_addr failed */
    }
    if (mapp)
	*mapp = map;			/* pass back map used */

    return res;				/* pciio_dmamap_addr succeeded */
}

void
pciio_dmamap_drain(pciio_dmamap_t map)
{
    DMAMAP_FUNC(map, dmamap_drain)
	(CAST_DMAMAP(map));
}

void
pciio_dmaaddr_drain(vertex_hdl_t dev, paddr_t addr, size_t size)
{
    DEV_FUNC(dev, dmaaddr_drain)
	(dev, addr, size);
}

void
pciio_dmalist_drain(vertex_hdl_t dev, alenlist_t list)
{
    DEV_FUNC(dev, dmalist_drain)
	(dev, list);
}

/* =====================================================================
 *          INTERRUPT MANAGEMENT
 *
 *      Allow crosstalk devices to establish interrupts
 */

/*
 * Allocate resources required for an interrupt as specified in intr_desc.
 * Return resource handle in intr_hdl.
 */
pciio_intr_t
pciio_intr_alloc(vertex_hdl_t dev,	/* which Crosstalk device */
		 device_desc_t dev_desc,	/* device descriptor */
		 pciio_intr_line_t lines,	/* INTR line(s) to attach */
		 vertex_hdl_t owner_dev)
{					/* owner of this interrupt */
    return (pciio_intr_t) DEV_FUNC(dev, intr_alloc)
	(dev, dev_desc, lines, owner_dev);
}

/*
 * Free resources consumed by intr_alloc.
 */
void
pciio_intr_free(pciio_intr_t intr_hdl)
{
    INTR_FUNC(intr_hdl, intr_free)
	(CAST_INTR(intr_hdl));
}

/*
 * Associate resources allocated with a previous pciio_intr_alloc call with the
 * described handler, arg, name, etc.
 *
 * Returns 0 on success, returns <0 on failure.
 */
int
pciio_intr_connect(pciio_intr_t intr_hdl,
		intr_func_t intr_func, intr_arg_t intr_arg)	/* pciio intr resource handle */
{
    return INTR_FUNC(intr_hdl, intr_connect)
	(CAST_INTR(intr_hdl), intr_func, intr_arg);
}

/*
 * Disassociate handler with the specified interrupt.
 */
void
pciio_intr_disconnect(pciio_intr_t intr_hdl)
{
    INTR_FUNC(intr_hdl, intr_disconnect)
	(CAST_INTR(intr_hdl));
}

/*
 * Return a hwgraph vertex that represents the CPU currently
 * targeted by an interrupt.
 */
vertex_hdl_t
pciio_intr_cpu_get(pciio_intr_t intr_hdl)
{
    return INTR_FUNC(intr_hdl, intr_cpu_get)
	(CAST_INTR(intr_hdl));
}

void
pciio_slot_func_to_name(char		       *name,
			pciio_slot_t		slot,
			pciio_function_t	func)
{
    /*
     * standard connection points:
     *
     * PCIIO_SLOT_NONE:	.../pci/direct
     * PCIIO_FUNC_NONE: .../pci/<SLOT>			ie. .../pci/3
     * multifunction:   .../pci/<SLOT><FUNC>		ie. .../pci/3c
     */

    if (slot == PCIIO_SLOT_NONE)
	sprintf(name, EDGE_LBL_DIRECT);
    else if (func == PCIIO_FUNC_NONE)
	sprintf(name, "%d", slot);
    else
	sprintf(name, "%d%c", slot, 'a'+func);
}

/*
 * pciio_cardinfo_get
 *
 * Get the pciio info structure corresponding to the
 * specified PCI "slot" (we like it when the same index
 * number is used for the PCI IDSEL, the REQ/GNT pair,
 * and the interrupt line being used for INTA. We like
 * it so much we call it the slot number).
 */
static pciio_info_t
pciio_cardinfo_get(
		      vertex_hdl_t pciio_vhdl,
		      pciio_slot_t pci_slot)
{
    char                    namebuf[16];
    pciio_info_t	    info = 0;
    vertex_hdl_t	    conn;

    pciio_slot_func_to_name(namebuf, pci_slot, PCIIO_FUNC_NONE);
    if (GRAPH_SUCCESS ==
	hwgraph_traverse(pciio_vhdl, namebuf, &conn)) {
	info = pciio_info_chk(conn);
	hwgraph_vertex_unref(conn);
    }

    return info;
}


/*
 * pciio_error_handler:
 * dispatch an error to the appropriate
 * pciio connection point, or process
 * it as a generic pci error.
 * Yes, the first parameter is the
 * provider vertex at the middle of
 * the bus; we get to the pciio connect
 * point using the ioerror widgetdev field.
 *
 * This function is called by the
 * specific PCI provider, after it has figured
 * out where on the PCI bus (including which slot,
 * if it can tell) the error came from.
 */
/*ARGSUSED */
int
pciio_error_handler(
		       vertex_hdl_t pciio_vhdl,
		       int error_code,
		       ioerror_mode_t mode,
		       ioerror_t *ioerror)
{
    pciio_info_t            pciio_info;
    vertex_hdl_t            pconn_vhdl;
    pciio_slot_t            slot;

    int                     retval;

#if DEBUG && ERROR_DEBUG
    printk("%v: pciio_error_handler\n", pciio_vhdl);
#endif

    IOERR_PRINTF(printk(KERN_NOTICE "%v: PCI Bus Error: Error code: %d Error mode: %d\n",
			 pciio_vhdl, error_code, mode));

    /* If there is an error handler sitting on
     * the "no-slot" connection point, give it
     * first crack at the error. NOTE: it is
     * quite possible that this function may
     * do further refining of the ioerror.
     */
    pciio_info = pciio_cardinfo_get(pciio_vhdl, PCIIO_SLOT_NONE);
    if (pciio_info && pciio_info->c_efunc) {
	pconn_vhdl = pciio_info_dev_get(pciio_info);

	retval = pciio_info->c_efunc
	    (pciio_info->c_einfo, error_code, mode, ioerror);
	if (retval != IOERROR_UNHANDLED)
	    return retval;
    }

    /* Is the error associated with a particular slot?
     */
    if (IOERROR_FIELDVALID(ioerror, widgetdev)) {
	short widgetdev;
	/*
	 * NOTE : 
	 * widgetdev is a 4byte value encoded as slot in the higher order
	 * 2 bytes and function in the lower order 2 bytes.
	 */
	IOERROR_GETVALUE(widgetdev, ioerror, widgetdev);
	slot = pciio_widgetdev_slot_get(widgetdev);

	/* If this slot has an error handler,
	 * deliver the error to it.
	 */
	pciio_info = pciio_cardinfo_get(pciio_vhdl, slot);
	if (pciio_info != NULL) {
	    if (pciio_info->c_efunc != NULL) {

		pconn_vhdl = pciio_info_dev_get(pciio_info);

		retval = pciio_info->c_efunc
		    (pciio_info->c_einfo, error_code, mode, ioerror);
		if (retval != IOERROR_UNHANDLED)
		    return retval;
	    }
	}
    }

    return (mode == MODE_DEVPROBE)
	? IOERROR_HANDLED	/* probes are OK */
	: IOERROR_UNHANDLED;	/* otherwise, foo! */
}

/* =====================================================================
 *          CONFIGURATION MANAGEMENT
 */

/*
 * Startup a crosstalk provider
 */
void
pciio_provider_startup(vertex_hdl_t pciio_provider)
{
    DEV_FUNC(pciio_provider, provider_startup)
	(pciio_provider);
}

/*
 * Shutdown a crosstalk provider
 */
void
pciio_provider_shutdown(vertex_hdl_t pciio_provider)
{
    DEV_FUNC(pciio_provider, provider_shutdown)
	(pciio_provider);
}

/*
 * Read value of configuration register
 */
uint64_t
pciio_config_get(vertex_hdl_t	dev,
		 unsigned	reg,
		 unsigned	size)
{
    uint64_t	value = 0;
    unsigned	shift = 0;

    /* handle accesses that cross words here,
     * since that's common code between all
     * possible providers.
     */
    while (size > 0) {
	unsigned	biw = 4 - (reg&3);
	if (biw > size)
	    biw = size;

	value |= DEV_FUNC(dev, config_get)
	    (dev, reg, biw) << shift;

	shift += 8*biw;
	reg += biw;
	size -= biw;
    }
    return value;
}

/*
 * Change value of configuration register
 */
void
pciio_config_set(vertex_hdl_t	dev,
		 unsigned	reg,
		 unsigned	size,
		 uint64_t	value)
{
    /* handle accesses that cross words here,
     * since that's common code between all
     * possible providers.
     */
    while (size > 0) {
	unsigned	biw = 4 - (reg&3);
	if (biw > size)
	    biw = size;
	    
	DEV_FUNC(dev, config_set)
	    (dev, reg, biw, value);
	reg += biw;
	size -= biw;
	value >>= biw * 8;
    }
}

/* =====================================================================
 *          GENERIC PCI SUPPORT FUNCTIONS
 */

/*
 * Issue a hardware reset to a card.
 */
int
pciio_reset(vertex_hdl_t dev)
{
    return DEV_FUNC(dev, reset) (dev);
}

/****** Generic pci slot information interfaces ******/

pciio_info_t
pciio_info_chk(vertex_hdl_t pciio)
{
    arbitrary_info_t        ainfo = 0;

    hwgraph_info_get_LBL(pciio, INFO_LBL_PCIIO, &ainfo);
    return (pciio_info_t) ainfo;
}

pciio_info_t
pciio_info_get(vertex_hdl_t pciio)
{
    pciio_info_t            pciio_info;

    pciio_info = (pciio_info_t) hwgraph_fastinfo_get(pciio);

#ifdef DEBUG_PCIIO
    {
	int pos;
	char dname[256];
	pos = devfs_generate_path(pciio, dname, 256);
	printk("%s : path= %s\n", __FUNCTION__, &dname[pos]);
    }
#endif /* DEBUG_PCIIO */

    if ((pciio_info != NULL) &&
        (pciio_info->c_fingerprint != pciio_info_fingerprint)
        && (pciio_info->c_fingerprint != NULL)) {

        return((pciio_info_t)-1); /* Should panic .. */
    }

    return pciio_info;
}

void
pciio_info_set(vertex_hdl_t pciio, pciio_info_t pciio_info)
{
    if (pciio_info != NULL)
	pciio_info->c_fingerprint = pciio_info_fingerprint;
    hwgraph_fastinfo_set(pciio, (arbitrary_info_t) pciio_info);

    /* Also, mark this vertex as a PCI slot
     * and use the pciio_info, so pciio_info_chk
     * can work (and be fairly efficient).
     */
    hwgraph_info_add_LBL(pciio, INFO_LBL_PCIIO,
			 (arbitrary_info_t) pciio_info);
}

vertex_hdl_t
pciio_info_dev_get(pciio_info_t pciio_info)
{
    return (pciio_info->c_vertex);
}


pciio_slot_t
pciio_info_slot_get(pciio_info_t pciio_info)
{
    return (pciio_info->c_slot);
}

vertex_hdl_t
pciio_info_master_get(pciio_info_t pciio_info)
{
    return (pciio_info->c_master);
}

arbitrary_info_t
pciio_info_mfast_get(pciio_info_t pciio_info)
{
    return (pciio_info->c_mfast);
}

pciio_provider_t       *
pciio_info_pops_get(pciio_info_t pciio_info)
{
    return (pciio_info->c_pops);
}


/* =====================================================================
 *          GENERIC PCI INITIALIZATION FUNCTIONS
 */

/*
 *    pciioattach: called for each vertex in the graph
 *      that is a PCI provider.
 */
/*ARGSUSED */
int
pciio_attach(vertex_hdl_t pciio)
{
#if DEBUG && ATTACH_DEBUG
    char devname[MAXDEVNAME];
    printk("%s: pciio_attach\n", vertex_to_name(pciio, devname, MAXDEVNAME));
#endif
    return 0;
}

/*
 * Associate a set of pciio_provider functions with a vertex.
 */
void
pciio_provider_register(vertex_hdl_t provider, pciio_provider_t *pciio_fns)
{
    hwgraph_info_add_LBL(provider, INFO_LBL_PFUNCS, (arbitrary_info_t) pciio_fns);
}

/*
 * Disassociate a set of pciio_provider functions with a vertex.
 */
void
pciio_provider_unregister(vertex_hdl_t provider)
{
    arbitrary_info_t        ainfo;

    hwgraph_info_remove_LBL(provider, INFO_LBL_PFUNCS, (long *) &ainfo);
}

/*
 * Obtain a pointer to the pciio_provider functions for a specified Crosstalk
 * provider.
 */
pciio_provider_t       *
pciio_provider_fns_get(vertex_hdl_t provider)
{
    arbitrary_info_t        ainfo = 0;

    (void) hwgraph_info_get_LBL(provider, INFO_LBL_PFUNCS, &ainfo);
    return (pciio_provider_t *) ainfo;
}

pciio_info_t
pciio_device_info_new(
		pciio_info_t pciio_info,
		vertex_hdl_t master,
		pciio_slot_t slot,
		pciio_function_t func,
		pciio_vendor_id_t vendor_id,
		pciio_device_id_t device_id)
{
    if (!pciio_info)
	NEW(pciio_info);
    ASSERT(pciio_info != NULL);

    pciio_info->c_slot = slot;
    pciio_info->c_func = func;
    pciio_info->c_vendor = vendor_id;
    pciio_info->c_device = device_id;
    pciio_info->c_master = master;
    pciio_info->c_mfast = hwgraph_fastinfo_get(master);
    pciio_info->c_pops = pciio_provider_fns_get(master);
    pciio_info->c_efunc = 0;
    pciio_info->c_einfo = 0;

    return pciio_info;
}

void
pciio_device_info_free(pciio_info_t pciio_info)
{
    /* NOTE : pciio_info is a structure within the pcibr_info
     *	      and not a pointer to memory allocated on the heap !!
     */
    memset((char *)pciio_info, 0, sizeof(pciio_info));
}

vertex_hdl_t
pciio_device_info_register(
		vertex_hdl_t connectpt,		/* vertex at center of bus */
		pciio_info_t pciio_info)	/* details about the connectpt */
{
    char		name[32];
    vertex_hdl_t	pconn;
    int device_master_set(vertex_hdl_t, vertex_hdl_t);

    pciio_slot_func_to_name(name,
			    pciio_info->c_slot,
			    pciio_info->c_func);

    if (GRAPH_SUCCESS !=
	hwgraph_path_add(connectpt, name, &pconn))
	return pconn;

    pciio_info->c_vertex = pconn;
    pciio_info_set(pconn, pciio_info);
#ifdef DEBUG_PCIIO
    {
	int pos;
	char dname[256];
	pos = devfs_generate_path(pconn, dname, 256);
	printk("%s : pconn path= %s \n", __FUNCTION__, &dname[pos]);
    }
#endif /* DEBUG_PCIIO */

    /*
     * create link to our pci provider
     */

    device_master_set(pconn, pciio_info->c_master);
    return pconn;
}

void
pciio_device_info_unregister(vertex_hdl_t connectpt,
			     pciio_info_t pciio_info)
{
    char		name[32];
    vertex_hdl_t	pconn;

    if (!pciio_info)
	return;

    pciio_slot_func_to_name(name,
			    pciio_info->c_slot,
			    pciio_info->c_func);

    hwgraph_edge_remove(connectpt,name,&pconn);
    pciio_info_set(pconn,0);

    /* Remove the link to our pci provider */
    hwgraph_edge_remove(pconn, EDGE_LBL_MASTER, NULL);


    hwgraph_vertex_unref(pconn);
    hwgraph_vertex_destroy(pconn);
    
}
/* Add the pci card inventory information to the hwgraph
 */
static void
pciio_device_inventory_add(vertex_hdl_t pconn_vhdl)
{
    pciio_info_t	pciio_info = pciio_info_get(pconn_vhdl);

    ASSERT(pciio_info);
    ASSERT(pciio_info->c_vertex == pconn_vhdl);

    /* Donot add inventory  for non-existent devices */
    if ((pciio_info->c_vendor == PCIIO_VENDOR_ID_NONE)	||
	(pciio_info->c_device == PCIIO_DEVICE_ID_NONE))
	return;
    device_inventory_add(pconn_vhdl,INV_IOBD,INV_PCIADAP,
			 pciio_info->c_vendor,pciio_info->c_device,
			 pciio_info->c_slot);
}

/*ARGSUSED */
int
pciio_device_attach(vertex_hdl_t pconn,
		    int          drv_flags)
{
    pciio_info_t            pciio_info;
    pciio_vendor_id_t       vendor_id;
    pciio_device_id_t       device_id;


    pciio_device_inventory_add(pconn);
    pciio_info = pciio_info_get(pconn);

    vendor_id = pciio_info->c_vendor;
    device_id = pciio_info->c_device;

    /* we don't start attaching things until
     * all the driver init routines (including
     * pciio_init) have been called; so we
     * can assume here that we have a registry.
     */

    return(cdl_add_connpt(vendor_id, device_id, pconn, drv_flags));
}

int
pciio_device_detach(vertex_hdl_t pconn,
		    int          drv_flags)
{
    return(0);
}

/*
 * Allocate space from the specified PCI window mapping resource.  On
 * success record information about the allocation in the supplied window
 * allocation cookie (if non-NULL) and return the address of the allocated
 * window.  On failure return NULL.
 *
 * The "size" parameter is usually from a PCI device's Base Address Register
 * (BAR) decoder.  As such, the allocation must be aligned to be a multiple of
 * that.  The "align" parameter acts as a ``minimum alignment'' allocation
 * constraint.  The alignment contraint reflects system or device addressing
 * restrictions such as the inability to share higher level ``windows''
 * between devices, etc.  The returned PCI address allocation will be a
 * multiple of the alignment constraint both in alignment and size.  Thus, the
 * returned PCI address block is aligned to the maximum of the requested size
 * and alignment.
 */
iopaddr_t
pciio_device_win_alloc(struct resource *root_resource,
		       pciio_win_alloc_t win_alloc,
		       size_t start, size_t size, size_t align)
{

	struct resource *new_res;
	int status = 0;

	new_res = (struct resource *) kmalloc( sizeof(struct resource), GFP_KERNEL);

	if (start > 0) {
		status = allocate_resource( root_resource, new_res,
			size, start /* Min start addr. */,
			(start + size) - 1, 1,
			NULL, NULL);
	} else {
		if (size > align)
			align = size;
		status = allocate_resource( root_resource, new_res,
				    size, align /* Min start addr. */,
				    root_resource->end, align,
				    NULL, NULL);
	}

	if (status) {
		kfree(new_res);
		return((iopaddr_t) NULL);
	}

	/*
	 * If a window allocation cookie has been supplied, use it to keep
	 * track of all the allocated space assigned to this window.
	 */
	if (win_alloc) {
		win_alloc->wa_resource = new_res;
		win_alloc->wa_base = new_res->start;
		win_alloc->wa_pages = size;
	}

	return new_res->start;;
}

/*
 * Free the specified window allocation back into the PCI window mapping
 * resource.  As noted above, we keep page addresses offset by 1 ...
 */
void
pciio_device_win_free(pciio_win_alloc_t win_alloc)
{

	int status = 0;

	if (win_alloc->wa_resource) {
		status = release_resource(win_alloc->wa_resource);
		if (!status)
			kfree(win_alloc->wa_resource);
		else
			BUG();
	}
}

/*
 * pciio_error_register:
 * arrange for a function to be called with
 * a specified first parameter plus other
 * information when an error is encountered
 * and traced to the pci slot corresponding
 * to the connection point pconn.
 *
 * may also be called with a null function
 * pointer to "unregister" the error handler.
 *
 * NOTE: subsequent calls silently overwrite
 * previous data for this vertex. We assume that
 * cooperating drivers, well, cooperate ...
 */
void
pciio_error_register(vertex_hdl_t pconn,
		     error_handler_f *efunc,
		     error_handler_arg_t einfo)
{
    pciio_info_t            pciio_info;

    pciio_info = pciio_info_get(pconn);
    ASSERT(pciio_info != NULL);
    pciio_info->c_efunc = efunc;
    pciio_info->c_einfo = einfo;
}

/*
 * Check if any device has been found in this slot, and return
 * true or false
 * vhdl is the vertex for the slot
 */
int
pciio_slot_inuse(vertex_hdl_t pconn_vhdl)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);

    ASSERT(pciio_info);
    ASSERT(pciio_info->c_vertex == pconn_vhdl);
    if (pciio_info->c_vendor) {
	/*
	 * Non-zero value for vendor indicate
	 * a board being found in this slot.
	 */
	return 1;
    }
    return 0;
}

int
pciio_dma_enabled(vertex_hdl_t pconn_vhdl)
{
	return DEV_FUNC(pconn_vhdl, dma_enabled)(pconn_vhdl);
}

int
pciio_info_type1_get(pciio_info_t pci_info)
{
	return(0);
}
