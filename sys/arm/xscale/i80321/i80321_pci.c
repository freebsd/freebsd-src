/*	$NetBSD: i80321_pci.c,v 1.4 2003/07/15 00:24:54 lukem Exp $	*/

/*-
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI configuration support for i80321 I/O Processor chip.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/i80321/i80321_pci.c,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <machine/pmap.h>

#include <arm/xscale/i80321/i80321reg.h>
#include <arm/xscale/i80321/i80321var.h>
#include <arm/xscale/i80321/i80321_intr.h>

#include <dev/pci/pcib_private.h>
#include "pcib_if.h"

#include <dev/pci/pcireg.h>
extern struct i80321_softc *i80321_softc;

static int
i80321_pci_probe(device_t dev)
{
	device_set_desc(dev, "i80321 PCI bus");
	return (0);
}

static int
i80321_pci_attach(device_t dev)
{

	uint32_t busno;                                       
	struct i80321_pci_softc *sc = device_get_softc(dev);

	sc->sc_st = i80321_softc->sc_st;
	sc->sc_atu_sh = i80321_softc->sc_atu_sh;
	busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCIXSR);
	busno = PCIXSR_BUSNO(busno);
	if (busno == 0xff)
		busno = 0;
	sc->sc_dev = dev;
	sc->sc_busno = busno;
	sc->sc_pciio = &i80321_softc->sc_pci_iot;
	sc->sc_pcimem = &i80321_softc->sc_pci_memt;
	sc->sc_mem = i80321_softc->sc_owin[0].owin_xlate_lo +
	    VERDE_OUT_XLATE_MEM_WIN_SIZE;
	
	sc->sc_io = i80321_softc->sc_iow_vaddr;
	/* Initialize memory and i/o rmans. */
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "I80321 PCI I/O Ports";
	if (rman_init(&sc->sc_io_rman) != 0 ||
		rman_manage_region(&sc->sc_io_rman, 
		sc->sc_io, 
		    sc->sc_io + 
		    VERDE_OUT_XLATE_IO_WIN_SIZE) != 0) {
		panic("i80321_pci_probe: failed to set up I/O rman");
	}
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "I80321 PCI Memory";
	if (rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, 
	    0, VERDE_OUT_XLATE_MEM_WIN_SIZE) != 0) {
		panic("i80321_pci_probe: failed to set up memory rman");
	}
	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "i80321 PCI IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, 26, 32) != 0)
		panic("i80321_pci_probe: failed to set up IRQ rman");
	device_add_child(dev, "pci",busno);
	return (bus_generic_attach(dev));
}

static int
i80321_pci_maxslots(device_t dev)
{
	return (PCI_SLOTMAX);
}



static int
i80321_pci_conf_setup(struct i80321_pci_softc *sc, int bus, int slot, int func,
    int reg, uint32_t *addr)
{
	uint32_t busno;

	busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCIXSR);
	busno = PCIXSR_BUSNO(busno);
	if (busno == 0xff)
		busno = 0;

	/*
	 * If the bus # is the same as our own, then use Type 0 cycles,
	 * else use Type 1.
	 *
	 * XXX We should filter out all non-private devices here!
	 * XXX How does private space interact with PCI-PCI bridges?
	 */
	if (bus == busno) {
		if (slot > (31 - 16))
			return (1);
		/*
		 * NOTE: PCI-X requires that that devices updated their
		 * PCIXSR on every config write with the device number
		 * specified in AD[15:11].  If we don't set this field,
		 * each device could end of thinking it is at device 0,
		 * which can cause a number of problems.  Doing this
		 * unconditionally should be OK when only PCI devices
		 * are present.
		 */
		bus &= 0xff;
		slot &= 0x1f;
		func &= 0x07;
		
		*addr = (1U << (slot + 16)) |
		    (slot << 11) | (func << 8) | reg;
	} else {
		*addr = (bus << 16) | (slot << 11) | (func << 8) | reg | 1;
	}

	return (0);
}

static u_int32_t
i80321_pci_read_config(device_t dev, int bus, int slot, int func, int reg,
    int bytes)
{
	struct i80321_pci_softc *sc = device_get_softc(dev);
	uint32_t isr;
	uint32_t addr;
	u_int32_t ret = 0;
	vm_offset_t va;
	int err = 0;
	if (i80321_pci_conf_setup(sc, bus, slot, func, reg & ~3, &addr))
		return (-1);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OCCAR,
	    addr);

	va = sc->sc_atu_sh;
	switch (bytes) {
	case 1:
		err = badaddr_read((void*)(va + ATU_OCCDR + (reg & 3)), 1, &ret);
		break;
	case 2:
		err = badaddr_read((void*)(va + ATU_OCCDR + (reg & 3)), 2, &ret);
		break;
	case 4:
		err = badaddr_read((void *)(va + ATU_OCCDR), 4, &ret);
		break;
	default:
		printf("i80321_read_config: invalid size %d\n", bytes);
		ret = -1;
	}
	if (err) {

		isr = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_ATUISR);
		bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_ATUISR,
		    isr & (ATUISR_P_SERR_DET|ATUISR_PMA|ATUISR_PTAM|
		    ATUISR_PTAT|ATUISR_PMPE));
		return (-1);
	}
	return (ret);
}

static void
i80321_pci_write_config(device_t dev, int bus, int slot, int func, int reg,
    u_int32_t data, int bytes)
{
	struct i80321_pci_softc *sc = device_get_softc(dev);
	uint32_t addr;

	if (i80321_pci_conf_setup(sc, bus, slot, func, reg & ~3, &addr))
		return;


	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OCCAR,
	    addr);
	switch (bytes) {
	case 1:
		bus_space_write_1(sc->sc_st, sc->sc_atu_sh, ATU_OCCDR +
		    (reg & 3), data);
		break;
	case 2:
		bus_space_write_2(sc->sc_st, sc->sc_atu_sh, ATU_OCCDR +
		    (reg & 3), data);
		break;
	case 4:
		bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OCCDR, data);
		break;
	default:
		printf("i80321_pci_write_config: Invalid size : %d\n", bytes);
	}

}

static int
i80321_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct i80321_pci_softc *sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = 0;
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_busno;
		return (0);
		
	}
	return (ENOENT);
}

static int
i80321_write_ivar(device_t dev, device_t child, int which, uintptr_t result)
{
	struct i80321_pci_softc * sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		return (EINVAL);
	case PCIB_IVAR_BUS:
		sc->sc_busno = result;
		return (0);
	}
	return (ENOENT);
}

static struct resource *
i80321_pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct i80321_pci_softc *sc = device_get_softc(bus);	
	struct resource *rv;
	struct rman *rm;
	bus_space_tag_t bt = NULL;
	bus_space_handle_t bh = 0;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->sc_irq_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		bt = sc->sc_pcimem;
		bh = (start >= 0x80000000 && start < 0x84000000) ? 0x80000000 :
		    sc->sc_mem;
		start &= (0x1000000 - 1);
		end &= (0x1000000 - 1);
		break;
	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		bt = sc->sc_pciio;
		bh = sc->sc_io;
		if (start < sc->sc_io) {
			start = start - 0x90000000 + sc->sc_io;
			end = end - 0x90000000 + sc->sc_io;
		}
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);
	rman_set_rid(rv, *rid);
	if (type != SYS_RES_IRQ) {
		if (type == SYS_RES_MEMORY)
			bh += (rman_get_start(rv));
		rman_set_bustag(rv, bt);
		rman_set_bushandle(rv, bh);
		if (flags & RF_ACTIVE) {
			if (bus_activate_resource(child, type, *rid, rv)) {
				rman_release_resource(rv);
				return (NULL);
			}
		} 
	}
	return (rv);
}

static int
i80321_pci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	u_long p;
	int error;
	
	if (type == SYS_RES_MEMORY) {
		error = bus_space_map(rman_get_bustag(r),
		    rman_get_bushandle(r), rman_get_size(r), 0, &p);
		if (error) 
			return (error);
		rman_set_bushandle(r, p);
	
	}
	return (rman_activate_resource(r));
}

static int
i80321_pci_setup_intr(device_t dev, device_t child,
    struct resource *ires, int flags, driver_filter_t *filt, 
    driver_intr_t *intr, void *arg, void **cookiep)    
{
	return (BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    filt, intr, arg, cookiep));
}

static int
i80321_pci_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}

static device_method_t i80321_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		i80321_pci_probe),
	DEVMETHOD(device_attach,	i80321_pci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	i80321_read_ivar),
	DEVMETHOD(bus_write_ivar,	i80321_write_ivar),
	DEVMETHOD(bus_alloc_resource,	i80321_pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, i80321_pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	i80321_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr,	i80321_pci_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	i80321_pci_maxslots),
	DEVMETHOD(pcib_read_config,	i80321_pci_read_config),
	DEVMETHOD(pcib_write_config,	i80321_pci_write_config),
	DEVMETHOD(pcib_route_interrupt,	machdep_pci_route_interrupt),

	{0, 0}
};

static driver_t i80321_pci_driver = {
	"pcib",
	i80321_pci_methods,
	sizeof(struct i80321_pci_softc),
};

static devclass_t i80321_pci_devclass;

DRIVER_MODULE(ipci, iq, i80321_pci_driver, i80321_pci_devclass, 0, 0);
