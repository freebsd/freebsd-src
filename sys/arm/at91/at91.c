/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
 * Copyright (c) 2010 Greg Ansley.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#define	_ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_pmcvar.h>
#include <arm/at91/at91_aicreg.h>

static struct at91_softc *at91_softc;

static void at91_eoi(void *);

extern const struct pmap_devmap at91_devmap[];

uint32_t at91_chip_id;

#ifdef AT91C_MASTER_CLOCK
uint32_t at91_master_clock = AT91C_MASTER_CLOCK;
#else
uint32_t at91_master_clock;
#endif

static int
at91_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	vm_paddr_t pa, endpa;

	pa = trunc_page(bpa);
	if (pa >= 0xfff00000) {
		*bshp = pa - 0xf0000000 + 0xd0000000;
		return (0);
	}
	if (pa >= 0xdff00000)
		return (0);
	endpa = round_page(bpa + size);

	*bshp = (vm_offset_t)pmap_mapdev(pa, endpa - pa);
		       
	return (0);
}

static void
at91_bs_unmap(void *t, bus_space_handle_t h, bus_size_t size)
{
	vm_offset_t va, endva;

	va = trunc_page((vm_offset_t)t);
	endva = va + round_page(size);

	/* Free the kernel virtual mapping. */
	kmem_free(kernel_map, va, endva - va);
}

static int
at91_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;
	return (0);
}

static void
at91_barrier(void *t, bus_space_handle_t bsh, bus_size_t size, bus_size_t b, 
    int a)
{
}

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (NULL);
}

int
bus_dma_get_range_nb(void)
{
	return (0);
}

bs_protos(generic);
bs_protos(generic_armv4);

struct bus_space at91_bs_tag = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	at91_bs_map,
	at91_bs_unmap,
	at91_bs_subregion,

	/* allocation/deallocation */
	NULL,
	NULL,

	/* barrier */
	at91_barrier,

	/* read (single) */
	generic_bs_r_1,
	generic_armv4_bs_r_2,
	generic_bs_r_4,
	NULL,

	/* read multiple */
	generic_bs_rm_1,
	generic_armv4_bs_rm_2,
	generic_bs_rm_4,
	NULL,

	/* read region */
	generic_bs_rr_1,
	generic_armv4_bs_rr_2,
	generic_bs_rr_4,
	NULL,

	/* write (single) */
	generic_bs_w_1,
	generic_armv4_bs_w_2,
	generic_bs_w_4,
	NULL,

	/* write multiple */
	generic_bs_wm_1,
	generic_armv4_bs_wm_2,
	generic_bs_wm_4,
	NULL,

	/* write region */
	NULL,
	generic_armv4_bs_wr_2,
	generic_bs_wr_4,
	NULL,

	/* set multiple */
	NULL,
	NULL,
	NULL,
	NULL,

	/* set region */
	NULL,
	generic_armv4_bs_sr_2,
	generic_bs_sr_4,
	NULL,

	/* copy */
	NULL,
	generic_armv4_bs_c_2,
	NULL,
	NULL,

	/* read (single) stream */
	generic_bs_r_1,
	generic_armv4_bs_r_2,
	generic_bs_r_4,
	NULL,

	/* read multiple stream */
	generic_bs_rm_1,
	generic_armv4_bs_rm_2,
	generic_bs_rm_4,
	NULL,

	/* read region stream */
	generic_bs_rr_1,
	generic_armv4_bs_rr_2,
	generic_bs_rr_4,
	NULL,

	/* write (single) stream */
	generic_bs_w_1,
	generic_armv4_bs_w_2,
	generic_bs_w_4,
	NULL,

	/* write multiple stream */
	generic_bs_wm_1,
	generic_armv4_bs_wm_2,
	generic_bs_wm_4,
	NULL,

	/* write region stream */
	NULL,
	generic_armv4_bs_wr_2,
	generic_bs_wr_4,
	NULL,
};

static int
at91_probe(device_t dev)
{

	device_set_desc(dev, "AT91 device bus");
	arm_post_filter = at91_eoi;
	return (0);
}

static void
at91_identify(driver_t *drv, device_t parent)
{
	
	BUS_ADD_CHILD(parent, 0, "atmelarm", 0);
}

static int
at91_attach(device_t dev)
{
	struct at91_softc *sc = device_get_softc(dev);
	const struct pmap_devmap *pdevmap;

	at91_softc = sc;
	sc->sc_st = &at91_bs_tag;
	sc->sc_sh = AT91_BASE;
	sc->dev = dev;

	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "AT91 IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, 1, 31) != 0)
		panic("at91_attach: failed to set up IRQ rman");

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "AT91 Memory";
	if (rman_init(&sc->sc_mem_rman) != 0)
		panic("at91_attach: failed to set up memory rman");
	for ( pdevmap = at91_devmap; pdevmap->pd_va != 0; pdevmap++) {
		if (rman_manage_region(&sc->sc_mem_rman, pdevmap->pd_va,
		    pdevmap->pd_va + pdevmap->pd_size - 1) != 0)
			panic("at91_attach: failed to set up memory rman");
	}


	/* Our device list will be added automatically by the cpu device
	 * e.g. at91rm9200.c when it is identified. To ensure that the
	 * CPU and PMC are attached first any other "identified" devices 
	 * call BUS_ADD_CHILD(9) with an "order" of at least 2. */

	bus_generic_probe(dev);
	bus_generic_attach(dev);
	enable_interrupts(I32_bit | F32_bit);
	return (0);
}

static struct resource *
at91_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct at91_softc *sc = device_get_softc(dev);
	struct resource_list_entry *rle;
	struct at91_ivar *ivar = device_get_ivars(child);
	struct resource_list *rl = &ivar->resources;

	if (device_get_parent(child) != dev)
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));
	
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
			rman_set_bustag(rle->res, &at91_bs_tag);
			rman_set_bushandle(rle->res, start);
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

static struct resource_list *
at91_get_resource_list(device_t dev, device_t child)
{
	struct at91_ivar *ivar;

	ivar = device_get_ivars(child);
	return (&(ivar->resources));
}

static int
at91_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	struct resource_list *rl;
	struct resource_list_entry *rle;

	rl = at91_get_resource_list(dev, child);
	if (rl == NULL)
		return (EINVAL);
	rle = resource_list_find(rl, type, rid);
	if (rle == NULL)
		return (EINVAL);
	rman_release_resource(r);
	rle->res = NULL;
	return (0);
}

static int
at91_setup_intr(device_t dev, device_t child,
    struct resource *ires, int flags, driver_filter_t *filt, 
    driver_intr_t *intr, void *arg, void **cookiep)    
{
	struct at91_softc *sc = device_get_softc(dev);
	int error;

	if (rman_get_start(ires) == sc->sc_irq_system && filt == NULL)
		panic("All system interrupt ISRs must be FILTER");
	error = BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    filt, intr, arg, cookiep);
	if (error)
		return (error);

	bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_IECR,
	    1 << rman_get_start(ires));
	return (0);
}

static int
at91_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{
	struct at91_softc *sc = device_get_softc(dev);

	bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_IDCR, 
	    1 << rman_get_start(res));
	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}

static int
at91_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
#if 0
	u_long p;
	int error;
	
	if (type == SYS_RES_MEMORY) {
		error = bus_space_map(rman_get_bustag(r),
		    rman_get_bushandle(r), rman_get_size(r), 0, &p);
		if (error) 
			return (error);
		rman_set_bushandle(r, p);
	}
#endif	
	return (rman_activate_resource(r));
}

static int
at91_print_child(device_t dev, device_t child)
{
	struct at91_ivar *ivars;
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

void
arm_mask_irq(uintptr_t nb)
{
	
	bus_space_write_4(at91_softc->sc_st, 
	    at91_softc->sc_aic_sh, IC_IDCR, 1 << nb);
}

int
arm_get_next_irq(int last __unused)
{
	int status;
	int irq;
	
	irq = bus_space_read_4(at91_softc->sc_st,
	    at91_softc->sc_aic_sh, IC_IVR);
	status = bus_space_read_4(at91_softc->sc_st,
	    at91_softc->sc_aic_sh, IC_ISR);
	if (status == 0) {
		bus_space_write_4(at91_softc->sc_st,
		    at91_softc->sc_aic_sh, IC_EOICR, 1);
		return (-1);
	}
	return (irq);
}

void
arm_unmask_irq(uintptr_t nb)
{
	
	bus_space_write_4(at91_softc->sc_st, 
	at91_softc->sc_aic_sh, IC_IECR, 1 << nb);
	bus_space_write_4(at91_softc->sc_st, at91_softc->sc_aic_sh,
	    IC_EOICR, 0);
}

static void
at91_eoi(void *unused)
{
	bus_space_write_4(at91_softc->sc_st, at91_softc->sc_aic_sh,
	    IC_EOICR, 0);
}

static device_method_t at91_methods[] = {
	DEVMETHOD(device_probe, at91_probe),
	DEVMETHOD(device_attach, at91_attach),
	DEVMETHOD(device_identify, at91_identify),

	DEVMETHOD(bus_alloc_resource, at91_alloc_resource),
	DEVMETHOD(bus_setup_intr, at91_setup_intr),
	DEVMETHOD(bus_teardown_intr, at91_teardown_intr),
	DEVMETHOD(bus_activate_resource, at91_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_get_resource_list,at91_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_release_resource,	at91_release_resource),
	DEVMETHOD(bus_print_child,	at91_print_child),

	{0, 0},
};

static driver_t at91_driver = {
	"atmelarm",
	at91_methods,
	sizeof(struct at91_softc),
};

static devclass_t at91_devclass;

DRIVER_MODULE(atmelarm, nexus, at91_driver, at91_devclass, 0, 0);
