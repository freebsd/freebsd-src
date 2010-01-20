/*-
 * Copyright (c) 2006 Olivier Houchard
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/types.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <machine/pmap.h>

#include <arm/xscale/i8134x/i81342reg.h>
#include <arm/xscale/i8134x/i81342var.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>
#include "pcib_if.h"

#include <dev/pci/pcireg.h>

static pcib_read_config_t i81342_pci_read_config;
static pcib_write_config_t i81342_pci_write_config;

static int
i81342_pci_probe(device_t dev)
{
	struct i81342_pci_softc *sc;
	
	sc = device_get_softc(dev);
	if (device_get_unit(dev) == 0) {
		device_set_desc(dev, "i81342 PCI-X bus");
		sc->sc_is_atux = 1;
	} else {
		device_set_desc(dev, "i81342 PCIe bus");
		sc->sc_is_atux = 0;
	}
	return (0);
}

#define PCI_MAPREG_MEM_PREFETCHABLE_MASK	0x00000008
#define PCI_MAPREG_MEM_TYPE_64BIT		0x00000004

static int
i81342_pci_attach(device_t dev)
{
	struct i81342_softc *parent_sc;
	struct i81342_pci_softc *sc;
	uint32_t memsize, memstart;
	uint32_t reg;
	int func;
	uint32_t busno;

	sc = device_get_softc(dev);
	parent_sc = device_get_softc(device_get_parent(dev));
	sc->sc_atu_sh = sc->sc_is_atux ? parent_sc->sc_atux_sh :
	    parent_sc->sc_atue_sh;
	sc->sc_st = parent_sc->sc_st;
	if (bus_space_read_4(sc->sc_st, parent_sc->sc_sh, IOP34X_ESSTSR0)
	    & IOP34X_INT_SEL_PCIX) {
		if (sc->sc_is_atux)
			func = 5;
		else
			func = 0;
	} else {
		if (sc->sc_is_atux)
			func = 0;
		else
			func = 5;
	}
	i81342_io_bs_init(&sc->sc_pciio, sc);
	i81342_mem_bs_init(&sc->sc_pcimem, sc);
	i81342_sdram_bounds(sc->sc_st, IOP34X_VADDR, &memstart, &memsize);
	if (sc->sc_is_atux) {
		reg = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCSR);
		if (reg & ATUX_P_RSTOUT) {
			bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_PCSR,
			    reg &~ ATUX_P_RSTOUT);
			DELAY(200);
		}
	}
	/* Setup the Inbound windows. */
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IABAR0, 0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IAUBAR0, 0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IALR0, 0);

	/* Set the mapping Physical address <=> PCI address */
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IABAR1,
	    memstart | PCI_MAPREG_MEM_PREFETCHABLE_MASK |
	    PCI_MAPREG_MEM_TYPE_64BIT);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IAUBAR1, 0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IALR1, ~(memsize - 1)
	     &~(0xfff));
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IATVR1, memstart);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IAUTVR1, 0);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IABAR2, 0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IAUBAR2, 0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IALR2, 0);

	/* Setup the Outbound IO Bar */
	if (sc->sc_is_atux)
		bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OIOBAR,
		    (IOP34X_PCIX_OIOBAR >> 4) | func);
	else
		bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OIOBAR,
		    (IOP34X_PCIE_OIOBAR >> 4) | func);

	/* Setup the Outbound windows */
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OUMBAR0, 0);
	if (sc->sc_is_atux)
		bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OUMBAR1,
		    (IOP34X_PCIX_OMBAR >> 32) | (func << ATU_OUMBAR_FUNC) |
		    ATU_OUMBAR_EN);
	else
		bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OUMBAR1,
		    (IOP34X_PCIE_OMBAR >> 32) | (func << ATU_OUMBAR_FUNC) |
		    ATU_OUMBAR_EN);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OUMWTVR1, 0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OUMBAR2, 0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_OUMBAR3, 0);

	/* Enable the outbound windows. */
	reg = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_CR);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_CR,
	    reg | ATU_CR_OUT_EN);
	
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_ISR,
	    bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_ISR) & ATUX_ISR_ERRMSK);
	/*
	 * Enable bus mastering, memory access, SERR, and parity
	 * checking on the ATU.
	 */
	if (sc->sc_is_atux) {
		busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCIXSR);
		busno = PCIXSR_BUSNO(busno);
	} else {
		busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCSR);
		busno = PCIE_BUSNO(busno);
	}
	reg = bus_space_read_2(sc->sc_st, sc->sc_atu_sh, ATU_CMD);
	reg |= PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN | PCIM_CMD_PERRESPEN |
	    PCIM_CMD_SERRESPEN;
	bus_space_write_2(sc->sc_st, sc->sc_atu_sh, ATU_CMD, reg);
	sc->sc_busno = busno;
	/* Initialize memory and i/o rmans. */
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "I81342 PCI I/O Ports";
	if (rman_init(&sc->sc_io_rman) != 0 ||
		rman_manage_region(&sc->sc_io_rman, 
		sc->sc_is_atux ? IOP34X_PCIX_OIOBAR_VADDR :
		IOP34X_PCIE_OIOBAR_VADDR,
		(sc->sc_is_atux ? IOP34X_PCIX_OIOBAR_VADDR :
		IOP34X_PCIE_OIOBAR_VADDR) + IOP34X_OIOBAR_SIZE) != 0) {
		panic("i81342_pci_probe: failed to set up I/O rman");
	}
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "I81342 PCI Memory";
	if (rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, 
	    0, 0xffffffff) != 0) {
		panic("i81342_pci_attach: failed to set up memory rman");
	}
	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "i81342 PCI IRQs";
	if (sc->sc_is_atux) {
		if (rman_init(&sc->sc_irq_rman) != 0 ||
		    rman_manage_region(&sc->sc_irq_rman, ICU_INT_XINT0, 
		    ICU_INT_XINT3) != 0)
			panic("i83142_pci_attach: failed to set up IRQ rman");
	} else {
		if (rman_init(&sc->sc_irq_rman) != 0 ||
		    rman_manage_region(&sc->sc_irq_rman, ICU_INT_ATUE_MA, 
		    ICU_INT_ATUE_MD) != 0)
			panic("i81342_pci_attach: failed to set up IRQ rman");

	}
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_ISR,
	    bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_ISR) & ATUX_ISR_ERRMSK);
	device_add_child(dev, "pci", busno);
	return (bus_generic_attach(dev));
}

static int
i81342_pci_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static void
i81342_pci_conf_setup(struct i81342_pci_softc *sc, int bus, int slot, int func,
    int reg, uint32_t *addr)
{
	uint32_t busno;

	if (sc->sc_is_atux) {
		busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCIXSR);
		busno = PCIXSR_BUSNO(busno);
	} else {
		busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCSR);
		busno = PCIE_BUSNO(busno);
	}
	bus &= 0xff;
	slot &= 0x1f;
	func &= 0x7;
	if (sc->sc_is_atux) {
		if (busno == bus)
			*addr = (1 << (slot + 16)) | (slot << 11) |
			    (func << 8) | reg;
		else
			*addr = (bus << 16) | (slot << 11) | (func << 11) | 
			    reg | 1;
	} else {
		*addr = (bus << 24) | (slot << 19) | (func << 16) | reg;
		if (bus != busno)
			*addr |= 1;
	}
}

static u_int32_t
i81342_pci_read_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct i81342_pci_softc *sc = device_get_softc(dev);
	uint32_t addr;
	uint32_t ret = 0;
	uint32_t isr;
	int err = 0;
	vm_offset_t va;

	i81342_pci_conf_setup(sc, bus, slot, func, reg & ~3, &addr);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, sc->sc_is_atux ?
	    ATUX_OCCAR : ATUE_OCCAR, addr);
	if (sc->sc_is_atux)
		va = sc->sc_atu_sh + ATUX_OCCDR;
	else
		va = sc->sc_atu_sh + ATUE_OCCDR;
	switch (bytes) {
	case 1:
		err = badaddr_read((void*)(va + (reg & 3)), 1, &ret);
		break;
	case 2:
		err = badaddr_read((void*)(va + (reg & 3)), 2, &ret);
		break;
	case 4:
		err = badaddr_read((void *)(va) , 4, &ret);
		break;
	default:
		printf("i81342_read_config: invalid size %d\n", bytes);
		ret = -1;
	}
	if (err) {
		isr = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_ISR);
		if (sc->sc_is_atux)
			isr &= ATUX_ISR_ERRMSK;
		else
			isr &= ATUE_ISR_ERRMSK;
		bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_ISR, isr);
		ret = -1;
	}

	return (ret);
}

static void
i81342_pci_write_config(device_t dev, u_int bus, u_int slot, u_int func, 
    u_int reg, u_int32_t data, int bytes)
{
	struct i81342_pci_softc *sc = device_get_softc(dev);
	uint32_t addr;
	vm_offset_t va;

	i81342_pci_conf_setup(sc, bus, slot, func, reg & ~3, &addr);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, sc->sc_is_atux ?
	    ATUX_OCCAR : ATUE_OCCAR, addr);
	va = sc->sc_is_atux ? ATUX_OCCDR : ATUE_OCCDR;
		switch (bytes) {
	case 1:
		bus_space_write_1(sc->sc_st, sc->sc_atu_sh, va + (reg & 3)
		    , data);
		break;
	case 2:
		bus_space_write_2(sc->sc_st, sc->sc_atu_sh, va + (reg & 3)
		    , data);
		break;
	case 4:
		bus_space_write_4(sc->sc_st, sc->sc_atu_sh, va, data);
		break;
	default:
		printf("i81342_pci_write_config: Invalid size : %d\n", bytes);
	}


}

static struct resource *
i81342_pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
   u_long start, u_long end, u_long count, u_int flags)
{
	struct i81342_pci_softc *sc = device_get_softc(bus);	
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
		bt = &sc->sc_pcimem;
		bh = 0;
		break;
	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		bt = &sc->sc_pciio;
		bh = sc->sc_is_atux ? IOP34X_PCIX_OIOBAR_VADDR :
		    IOP34X_PCIE_OIOBAR_VADDR;
		start += bh;
		end += bh;
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


	return (NULL);
}

static int
i81342_pci_activate_resource(device_t bus, device_t child, int type, int rid,
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
i81342_pci_setup_intr(device_t dev, device_t child, struct resource *ires,
    int flags, driver_filter_t *filt, driver_intr_t *intr, void *arg,
    void **cookiep)
{

	return (BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    filt, intr, arg, cookiep));
}



static int
i81342_pci_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}

static int
i81342_pci_route_interrupt(device_t pcib, device_t dev, int pin)
{
	struct i81342_pci_softc *sc;
	int device;
	
	device = pci_get_slot(dev);
	sc = device_get_softc(pcib);
	/* XXX: Is board specific */
	if (sc->sc_is_atux) {
		/* PCI-X */
		switch(device) {
		case 1:
			switch (pin) {
			case 1:
				return (ICU_INT_XINT1);
			case 2:
				return (ICU_INT_XINT2);
			case 3:
				return (ICU_INT_XINT3);
			case 4:
				return (ICU_INT_XINT0);
			default:
				break;
			}
		case 2:
			switch (pin) {
			case 1:
				return (ICU_INT_XINT2);
			case 2:
				return (ICU_INT_XINT3);
			case 3:
				return (ICU_INT_XINT2);
			case 4:
				return (ICU_INT_XINT3);
			default:
				break;
			}
		}
		
	} else {
		switch (pin) {
		case 1:
			return (ICU_INT_ATUE_MA);
		case 2:
			return (ICU_INT_ATUE_MB);
		case 3:
			return (ICU_INT_ATUE_MC);
		case 4:
			return (ICU_INT_ATUE_MD);
		default:
			break;
		}
	}
	printf("Warning: couldn't map %s IRQ for device %d pin %d\n",
	    sc->sc_is_atux ? "PCI-X" : "PCIe", device, pin);
	return (-1);
}

static int
i81342_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct i81342_pci_softc *sc = device_get_softc(dev);
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
i81342_write_ivar(device_t dev, device_t child, int which, uintptr_t result)
{
	struct i81342_pci_softc * sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		return (EINVAL);
	case PCIB_IVAR_BUS:
		sc->sc_busno = result;
		return (0);
	}
	return (ENOENT);
}

static device_method_t i81342_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		i81342_pci_probe),
	DEVMETHOD(device_attach,	i81342_pci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	i81342_read_ivar),
	DEVMETHOD(bus_write_ivar,	i81342_write_ivar),
	DEVMETHOD(bus_alloc_resource,	i81342_pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, i81342_pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	i81342_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr,	i81342_pci_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	i81342_pci_maxslots),
	DEVMETHOD(pcib_read_config,	i81342_pci_read_config),
	DEVMETHOD(pcib_write_config,	i81342_pci_write_config),
	DEVMETHOD(pcib_route_interrupt,	i81342_pci_route_interrupt),

	{0, 0}
};

static driver_t i81342_pci_driver = {
	"pcib",
	i81342_pci_methods,
	sizeof(struct i81342_pci_softc),
};

static devclass_t i81342_pci_devclass;

DRIVER_MODULE(ipci, iq, i81342_pci_driver, i81342_pci_devclass, 0, 0);
