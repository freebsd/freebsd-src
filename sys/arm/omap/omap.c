/*
 * Copyright (c) 2010
 *	Ben Gray <ben.r.gray@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ben Gray.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/kdb.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/bus.h>

#include <arm/omap/omapvar.h>

static int	omap_probe(device_t);
static void	omap_identify(driver_t *, device_t);
static int	omap_attach(device_t);

extern const struct pmap_devmap omap_devmap[];

/* Proto types for all the bus_space structure functions */
bs_protos(generic);
bs_protos(generic_armv7);

struct bus_space omap_bs_tag = {
	/* cookie */
	.bs_cookie	= (void *) 0,
	
	/* mapping/unmapping */
	.bs_map		= generic_bs_map,
	.bs_unmap	= generic_bs_unmap,
	.bs_subregion	= generic_bs_subregion,
	
	/* allocation/deallocation */
	.bs_alloc	= generic_bs_alloc,
	.bs_free	= generic_bs_free,
	
	/* barrier */
	.bs_barrier	= generic_bs_barrier,
	
	/* read (single) */
	.bs_r_1		= generic_bs_r_1,
	.bs_r_2		= generic_armv7_bs_r_2,
	.bs_r_4		= generic_bs_r_4,
	.bs_r_8		= NULL,
	
	/* read multiple */
	.bs_rm_1	= generic_bs_rm_1,
	.bs_rm_2	= generic_armv7_bs_rm_2,
	.bs_rm_4	= generic_bs_rm_4,
	.bs_rm_8	= NULL,
	
	/* read region */
	.bs_rr_1	= generic_bs_rr_1,
	.bs_rr_2	= generic_armv7_bs_rr_2,
	.bs_rr_4	= generic_bs_rr_4,
	.bs_rr_8	= NULL,
	
	/* write (single) */
	.bs_w_1		= generic_bs_w_1,
	.bs_w_2		= generic_armv7_bs_w_2,
	.bs_w_4		= generic_bs_w_4,
	.bs_w_8		= NULL,
	
	/* write multiple */
	.bs_wm_1	= generic_bs_wm_1,
	.bs_wm_2	= generic_armv7_bs_wm_2,
	.bs_wm_4	= generic_bs_wm_4,
	.bs_wm_8	= NULL,
	
	/* write region */
	.bs_wr_1	= generic_bs_wr_1,
	.bs_wr_2	= generic_armv7_bs_wr_2,
	.bs_wr_4	= generic_bs_wr_4,
	.bs_wr_8	= NULL,
	
	/* set multiple */
	/* XXX not implemented */
	
	/* set region */
	.bs_sr_1	= NULL,
	.bs_sr_2	= generic_armv7_bs_sr_2,
	.bs_sr_4	= generic_bs_sr_4,
	.bs_sr_8	= NULL,
	
	/* copy */
	.bs_c_1		= NULL,
	.bs_c_2		= generic_armv7_bs_c_2,
	.bs_c_4		= NULL,
	.bs_c_8		= NULL,
};

static void
omap_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "omap", 0);
}

/**
 *	omap_probe - driver probe function
 *	@dev: the root device
 * 
 *	Simply sets the name of this base driver
 */
static int
omap_probe(device_t dev)
{

	device_set_desc(dev, "TI OMAP");
	return (0);
}

/**
 * omap_attach
 */
static int
omap_attach(device_t dev)
{
	struct omap_softc *sc = device_get_softc(dev);
	const struct pmap_devmap *pdevmap;

	sc->sc_iotag = &omap_bs_tag;
	sc->sc_dev = dev;


	/* Set all interrupts as the resource */
	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "OMAP IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, 0, NIRQ) != 0) {
		panic("%s: failed to set up IRQ rman", __func__);
	}

	/* Setup the memory map based on initial device map in *_machdep.c */
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "OMAP Memory";

	if (rman_init(&sc->sc_mem_rman) != 0)
		panic("%s: failed to set up memory rman", __func__);

	for (pdevmap = omap_devmap; pdevmap->pd_va != 0; pdevmap++) {
		if (rman_manage_region(&sc->sc_mem_rman, pdevmap->pd_pa,
		                       pdevmap->pd_pa + pdevmap->pd_size - 1) != 0) {
			panic("%s: failed to set up memory regions", __func__);
		}
	}

	/* The device list will be created by the 'cpu' device when it is identified */
	bus_generic_probe(dev);
	bus_generic_attach(dev);
	enable_interrupts(I32_bit | F32_bit);
	return (0);
}

/**
 *	omap_devmap_phys2virt
 *
 *	This function will be called when bus_alloc_resource(...) if the memory
 *	region requested is in the range of the managed values set by
 *	rman_manage_region(...) above.
 *
 *	For SYS_RES_MEMORY resource types the omap_attach() calls rman_manage_region
 *	with the list of pyshical mappings defined in the omap_devmap region map.
 *	However because we are working with physical addresses, we need to convert
 *	the physical to virtual within this function and return the virtual address
 *	in the bus tag field.
 *
 */
static u_long
omap_devmap_phys2virt(u_long pa)
{
	const struct pmap_devmap *pdevmap;

	for (pdevmap = omap_devmap; pdevmap->pd_va != 0; pdevmap++) {
		if ((pa >= pdevmap->pd_pa) && (pa < (pdevmap->pd_pa + pdevmap->pd_size))) {
			return (pdevmap->pd_va + (pa - pdevmap->pd_pa));
		}
	}

	panic("%s: failed to find addr mapping for 0x%08lx", __func__, pa);
	return (0);
}

/**
 *	omap_alloc_resource
 *
 *	This function will be called when bus_alloc_resource(...) if the memory
 *	region requested is in the range of the managed values set by
 *	rman_manage_region(...) above.
 *
 *	For SYS_RES_MEMORY resource types the omap_attach() calls rman_manage_region
 *	with the list of pyshical mappings defined in the omap_devmap region map.
 *	However because we are working with physical addresses, we need to convert
 *	the physical to virtual within this function and return the virtual address
 *	in the bus tag field.
 *
 */
static struct resource *
omap_alloc_resource(device_t dev, device_t child, int type, int *rid,
                     u_long start, u_long end, u_long count, u_int flags)
{
	struct omap_softc *sc = device_get_softc(dev);
	struct resource_list_entry *rle;
	struct omap_ivar *ivar = device_get_ivars(child);
	struct resource_list *rl = &ivar->resources;

	/* If we aren't the parent pass it onto the actual parent */
	if (device_get_parent(child) != dev) {
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));
	}
	
	/* Find the resource in the list */
	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL)
		return (NULL);
	if (rle->res)
		panic("Resource rid %d type %d already in use", *rid, type);

	if (start == 0UL && end == ~0UL) {
		start = rle->start;
		count = ulmax(count, rle->count);
		end = ulmax(rle->end, start + count - 1);
	}

	switch (type)
	{
	case SYS_RES_IRQ:
		rle->res = rman_reserve_resource(&sc->sc_irq_rman,
		    start, end, count, flags, child);
		break;
	case SYS_RES_MEMORY:
		rle->res = rman_reserve_resource(&sc->sc_mem_rman,
		    start, end, count, flags, child);
		if (rle->res != NULL) {
			rman_set_bustag(rle->res, &omap_bs_tag);
			rman_set_bushandle(rle->res, omap_devmap_phys2virt(start));
		}
		break;
	}

	if (rle->res) {
		rle->start = rman_get_start(rle->res);
		rle->end = rman_get_end(rle->res);
		rle->count = count;
		rman_set_rid(rle->res, *rid);
	}
	return (rle->res);
}

/**
 * omap_get_resource_list
 *
 */
static struct resource_list *
omap_get_resource_list(device_t dev, device_t child)
{
	struct omap_ivar *ivar;

	ivar = device_get_ivars(child);
	return (&(ivar->resources));
}

/**
 * omap_release_resource
 *
 */
static int
omap_release_resource(device_t dev, device_t child, int type, int rid,
                       struct resource *r)
{
	struct resource_list *rl;
	struct resource_list_entry *rle;

	rl = omap_get_resource_list(dev, child);
	if (rl == NULL)
		return (EINVAL);
	
	rle = resource_list_find(rl, type, rid);
	if (rle == NULL)
		return (EINVAL);
	
	rman_release_resource(r);
	rle->res = NULL;
	return (0);
}

/**
 * omap_activate_resource
 *
 * 
 *
 */
static int
omap_activate_resource(device_t dev, device_t child, int type, int rid,
						 struct resource *r)
{

#if 0
	struct omap3_softc *sc = device_get_softc(dev);
	const struct hwvtrans *vtrans;
	uint32_t start = rman_get_start(r);
	uint32_t size = rman_get_size(r);
	
	if (type == SYS_RES_MEMORY) {
		vtrans = omap3_gethwvtrans(start, size);
		if (vtrans == NULL) {		/* NB: should not happen */
			device_printf(child, "%s: no mapping for 0x%lx:0x%lx\n",
						  __func__, start, size);
			return (ENOENT);
		}
		rman_set_bustag(r, sc->sc_iot);
		rman_set_bushandle(r, vtrans->vbase + (start - vtrans->hwbase));
	}
#endif
	return (rman_activate_resource(r));
}


/**
 * omap_deactivate_resource
 *
 */
static int
omap_deactivate_resource(device_t bus, device_t child, int type, int rid,
						   struct resource *r) 
{
	/* NB: no private resources, just deactive */
	return (rman_deactivate_resource(r));
}

static device_t
omap_add_child(device_t bus, u_int order, const char *name, int unit)
{
	device_t	child;
	struct omap_ivar *ivar;

	ivar = malloc(sizeof(struct omap_ivar), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!ivar)
		return (0);
	resource_list_init(&ivar->resources);

	child = device_add_child_ordered(bus, order, name, unit);
	if (child == NULL) {
		device_printf(bus, "failed to add child: %s%d\n", name, unit);
		return (0);
	}

	/* should we free this in nexus_child_detached? */
	device_set_ivars(child, ivar);

	return (child);
}



/**
 * omap_print_child
 *
 */
static int
omap_print_child(device_t dev, device_t child)
{
	struct omap_ivar *ivars;
	struct resource_list *rl;
	int retval = 0;

	ivars = device_get_ivars(child);
	rl = &ivars->resources;

	retval += bus_print_child_header(dev, child);

	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");
	if (device_get_flags(dev))
		retval += printf(" flags %#x", device_get_flags(dev));

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

/**
 *	omap_setup_intr - initialises and unmasks the IRQ.
 *
 *	RETURNS:
 *	0 on success
 */
int
omap_setup_intr(device_t dev, device_t child,
                struct resource *res, int flags, driver_filter_t *filt, 
                driver_intr_t *intr, void *arg, void **cookiep)    
{
	unsigned int i;

	BUS_SETUP_INTR(device_get_parent(dev), child, res, flags, filt, intr,
				   arg, cookiep);

	/* Enable all the interrupts in the range ... will probably be only one */
	for (i = rman_get_start(res); (i < NIRQ) && (i <= rman_get_end(res)); i++) {
		arm_unmask_irq(i);
	}
	
	return (0);
}

/**
 *	omap_teardown_intr 
 *
 *	RETURNS:
 *	0 on success
 */
int
omap_teardown_intr(device_t dev, device_t child, struct resource *res,
					 void *cookie)
{
	unsigned int i;

	/* Mask (disable) all the interrupts in the range ... will probably be only one */
	for (i = rman_get_start(res); (i < NIRQ) && (i <= rman_get_end(res)); i++) {
		arm_mask_irq(i);
	}
	
	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}



static device_method_t omap_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			omap_probe),
	DEVMETHOD(device_attach,		omap_attach),
	DEVMETHOD(device_identify,		omap_identify),
	
	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,		omap_alloc_resource),
	DEVMETHOD(bus_activate_resource,	omap_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	omap_deactivate_resource),
	DEVMETHOD(bus_get_resource_list,	omap_get_resource_list),
	DEVMETHOD(bus_set_resource,		bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,		bus_generic_rl_get_resource),
	DEVMETHOD(bus_release_resource,		omap_release_resource),

	DEVMETHOD(bus_setup_intr,		omap_setup_intr),
	DEVMETHOD(bus_teardown_intr,		omap_teardown_intr),
	
	DEVMETHOD(bus_add_child,		omap_add_child),
	DEVMETHOD(bus_print_child,		omap_print_child),

	{0, 0},
};

static driver_t omap_driver = {
	"omap",
	omap_methods,
	sizeof(struct omap_softc),
};
static devclass_t omap_devclass;

DRIVER_MODULE(omap, nexus, omap_driver, omap_devclass, 0, 0);
