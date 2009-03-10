/*	$NetBSD: ixp425_pci.c,v 1.5 2006/04/10 03:36:03 simonb Exp $ */

/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
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
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
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
#include <sys/malloc.h>
#define _ARM32_BUS_DMA_PRIVATE
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

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#include <dev/pci/pcib_private.h>
#include "pcib_if.h"

#include <dev/pci/pcireg.h>
extern struct ixp425_softc *ixp425_softc;

#define	PCI_CSR_WRITE_4(sc, reg, data)	\
	bus_write_4(sc->sc_csr, reg, data)

#define	PCI_CSR_READ_4(sc, reg)	\
	bus_read_4(sc->sc_csr, reg)

#define PCI_CONF_LOCK(s)	(s) = disable_interrupts(I32_bit)
#define PCI_CONF_UNLOCK(s)	restore_interrupts((s))

static device_probe_t ixppcib_probe;
static device_attach_t ixppcib_attach;
static bus_read_ivar_t ixppcib_read_ivar;
static bus_write_ivar_t ixppcib_write_ivar;
static bus_setup_intr_t ixppcib_setup_intr;
static bus_teardown_intr_t ixppcib_teardown_intr;
static bus_alloc_resource_t ixppcib_alloc_resource;
static bus_activate_resource_t ixppcib_activate_resource;
static bus_deactivate_resource_t ixppcib_deactivate_resource;
static bus_release_resource_t ixppcib_release_resource;
static pcib_maxslots_t ixppcib_maxslots;
static pcib_read_config_t ixppcib_read_config;
static pcib_write_config_t ixppcib_write_config;
static pcib_route_interrupt_t ixppcib_route_interrupt;

static int
ixppcib_probe(device_t dev)
{
	device_set_desc(dev, "IXP4XX PCI Bus");
        return (0);
}

static void
ixp425_pci_conf_reg_write(struct ixppcib_softc *sc, uint32_t reg,
    uint32_t data)
{
	PCI_CSR_WRITE_4(sc, PCI_CRP_AD_CBE, ((reg & ~3) | COMMAND_CRP_WRITE));
	PCI_CSR_WRITE_4(sc, PCI_CRP_AD_WDATA, data);
}

static int
ixppcib_attach(device_t dev)
{
	int rid;
	struct ixppcib_softc *sc;

	sc = device_get_softc(dev);

	rid = 0;
	sc->sc_csr = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    IXP425_PCI_HWBASE, IXP425_PCI_HWBASE + IXP425_PCI_SIZE,
	    IXP425_PCI_SIZE, RF_ACTIVE);
	if (sc->sc_csr == NULL)
		panic("cannot allocate PCI CSR registers");

	ixp425_md_attach(dev);
	/* always setup the base, incase another OS messes w/ it */
	PCI_CSR_WRITE_4(sc, PCI_PCIMEMBASE, 0x48494a4b);

	rid = 0;
	sc->sc_mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    IXP425_PCI_MEM_HWBASE, IXP425_PCI_MEM_HWBASE + IXP425_PCI_MEM_SIZE,
	    IXP425_PCI_MEM_SIZE, RF_ACTIVE);
	if (sc->sc_mem == NULL)
		panic("cannot allocate PCI MEM space");

	/* NB: PCI dma window is 64M so anything above must be bounced */
	if (bus_dma_tag_create(NULL, 1, 0, IXP425_AHB_OFFSET + 64 * 1024 * 1024,
	    BUS_SPACE_MAXADDR, NULL, NULL,  0xffffffff, 0xff, 0xffffffff, 0, 
	    NULL, NULL, &sc->sc_dmat))
		panic("couldn't create the PCI dma tag !");
	/* 
	 * The PCI bus can only address 64MB. However, due to the way our
	 * implementation of busdma works, busdma can't tell if a device
	 * is a PCI device or not. So defaults to the PCI dma tag, which
	 * restrict the DMA'able memory to the first 64MB, and explicitely
	 * create less restrictive tags for non-PCI devices.
	 */
	arm_root_dma_tag = sc->sc_dmat;
	/*
	 * Initialize the bus space tags.
	 */
	ixp425_io_bs_init(&sc->sc_pci_iot, sc);
	ixp425_mem_bs_init(&sc->sc_pci_memt, sc);

	sc->sc_dev = dev;

	/* Initialize memory and i/o rmans. */
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "IXP4XX PCI I/O Ports";
	if (rman_init(&sc->sc_io_rman) != 0 ||
		rman_manage_region(&sc->sc_io_rman, 0, 
	    	    IXP425_PCI_IO_SIZE) != 0) {
		panic("ixppcib_probe: failed to set up I/O rman");
	}

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "IXP4XX PCI Memory";
	if (rman_init(&sc->sc_mem_rman) != 0 ||
		rman_manage_region(&sc->sc_mem_rman, IXP425_PCI_MEM_HWBASE,
		    IXP425_PCI_MEM_HWBASE + IXP425_PCI_MEM_SIZE) != 0) {
		panic("ixppcib_probe: failed to set up memory rman");
	}

	/*
	 * PCI->AHB address translation
	 * 	begin at the physical memory start + OFFSET
	 */
	PCI_CSR_WRITE_4(sc, PCI_AHBMEMBASE,
	    (IXP425_AHB_OFFSET & 0xFF000000) +
	    ((IXP425_AHB_OFFSET & 0xFF000000) >> 8) +
	    ((IXP425_AHB_OFFSET & 0xFF000000) >> 16) +
	    ((IXP425_AHB_OFFSET & 0xFF000000) >> 24) +
	    0x00010203);
	
#define IXPPCIB_WRITE_CONF(sc, reg, val) \
	ixp425_pci_conf_reg_write(sc, reg, val)
	/* Write Mapping registers PCI Configuration Registers */
	/* Base Address 0 - 3 */
	IXPPCIB_WRITE_CONF(sc, PCI_MAPREG_BAR0, IXP425_AHB_OFFSET + 0x00000000);
	IXPPCIB_WRITE_CONF(sc, PCI_MAPREG_BAR1, IXP425_AHB_OFFSET + 0x01000000);
	IXPPCIB_WRITE_CONF(sc, PCI_MAPREG_BAR2, IXP425_AHB_OFFSET + 0x02000000);
	IXPPCIB_WRITE_CONF(sc, PCI_MAPREG_BAR3, IXP425_AHB_OFFSET + 0x03000000);
	
	/* Base Address 4 */
	IXPPCIB_WRITE_CONF(sc, PCI_MAPREG_BAR4, 0xffffffff);
	
	/* Base Address 5 */
	IXPPCIB_WRITE_CONF(sc, PCI_MAPREG_BAR5, 0x00000000);
	
	/* Assert some PCI errors */
	PCI_CSR_WRITE_4(sc, PCI_ISR, ISR_AHBE | ISR_PPE | ISR_PFE | ISR_PSE);
	
#ifdef __ARMEB__
	/*
	 * Set up byte lane swapping between little-endian PCI
	 * and the big-endian AHB bus
	 */
	PCI_CSR_WRITE_4(sc, PCI_CSR, CSR_IC | CSR_ABE | CSR_PDS);
#else
	PCI_CSR_WRITE_4(sc, PCI_CSR, CSR_IC | CSR_ABE);
#endif
	
	/*
	 * Enable bus mastering and I/O,memory access
	 */
	IXPPCIB_WRITE_CONF(sc, PCIR_COMMAND,
	    PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
	
	/*
	 * Wait some more to ensure PCI devices have stabilised.
	 */
	DELAY(50000);

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static int
ixppcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct ixppcib_softc *sc;

	sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = 0;
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_bus;
		return (0);
	}

	return (ENOENT);
}

static int
ixppcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct ixppcib_softc *sc;

	sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		return (EINVAL);
	case PCIB_IVAR_BUS:
		sc->sc_bus = value;
		return (0);
	}

	return (ENOENT);
}

static int
ixppcib_setup_intr(device_t dev, device_t child, struct resource *ires,
    int flags, driver_filter_t *filt, driver_intr_t *intr, void *arg, 
    void **cookiep)
{

	return (BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    filt, intr, arg, cookiep));
}

static int
ixppcib_teardown_intr(device_t dev, device_t child, struct resource *vec,
     void *cookie)
{

	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, vec, cookie));
}

static struct resource *
ixppcib_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct ixppcib_softc *sc = device_get_softc(bus);
	struct rman *rmanp;
	struct resource *rv;

	rv = NULL;
	switch (type) {
	case SYS_RES_IRQ:
		rmanp = &sc->sc_irq_rman;
		break;

	case SYS_RES_IOPORT:
		rmanp = &sc->sc_io_rman;
		break;

	case SYS_RES_MEMORY:
		rmanp = &sc->sc_mem_rman;
		break;

	default:
		return (rv);
	}

	rv = rman_reserve_resource(rmanp, start, end, count, flags & ~RF_ACTIVE,
	    child);
	if (rv == NULL)
		return (NULL);
	rman_set_rid(rv, *rid);
	if (flags & RF_ACTIVE) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
ixppcib_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r) 
{

	struct ixppcib_softc *sc = device_get_softc(bus);
  
	switch (type) {
	case SYS_RES_IOPORT:
		rman_set_bustag(r, &sc->sc_pci_iot);
		rman_set_bushandle(r, rman_get_start(r));
		break;
	case SYS_RES_MEMORY:
		rman_set_bustag(r, &sc->sc_pci_memt);
		rman_set_bushandle(r, rman_get_bushandle(sc->sc_mem) +
		    (rman_get_start(r) - IXP425_PCI_MEM_HWBASE));
		break;
	}
		
	return (rman_activate_resource(r));
}

static int
ixppcib_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r) 
{

	device_printf(bus, "%s called deactivate_resource (unexpected)\n",
	    device_get_nameunit(child));
	return (ENXIO);
}

static int
ixppcib_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	device_printf(bus, "%s called release_resource (unexpected)\n",
	    device_get_nameunit(child));
	return (ENXIO);
}

static void
ixppcib_conf_setup(struct ixppcib_softc *sc, int bus, int slot, int func,
    int reg)
{
	if (bus == 0) {
		/* configuration type 0 */
		PCI_CSR_WRITE_4(sc, PCI_NP_AD,
		    (1U << (32 - (slot & 0x1f))) |
		    ((func & 0x7) << 8) | (reg & ~3));
	} else {
		/* configuration type 1 */
		PCI_CSR_WRITE_4(sc, PCI_NP_AD,
		    (bus << 16) | (slot << 11) |
		    (func << 8) | (reg & ~3) | 1);
	}

}

static int
ixppcib_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static u_int32_t
ixppcib_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int bytes)
{
	struct ixppcib_softc *sc = device_get_softc(dev);
	u_int32_t data, ret;

	ixppcib_conf_setup(sc, bus, slot, func, reg & ~3);

	PCI_CSR_WRITE_4(sc, PCI_NP_CBE, COMMAND_NP_CONF_READ);
	ret = PCI_CSR_READ_4(sc, PCI_NP_RDATA);
	ret >>= (reg & 3) * 8;
	ret &= 0xffffffff >> ((4 - bytes) * 8);
#if 0
	device_printf(dev, "%s: %u:%u:%u %#x(%d) = %#x\n",
	    __func__, bus, slot, func, reg, bytes, ret);
#endif
	/* check & clear PCI abort */
	data = PCI_CSR_READ_4(sc, PCI_ISR);
	if (data & ISR_PFE) {
		PCI_CSR_WRITE_4(sc, PCI_ISR, ISR_PFE);
		return (-1);
	}
	return (ret);
}

static const int byteenables[] = { 0, 0x10, 0x30, 0x70, 0xf0 };

static void
ixppcib_write_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    u_int32_t val, int bytes)
{
	struct ixppcib_softc *sc = device_get_softc(dev);
	u_int32_t data;

#if 0
	device_printf(dev, "%s: %u:%u:%u %#x(%d) = %#x\n",
	    __func__, bus, slot, func, reg, bytes, val);
#endif
	ixppcib_conf_setup(sc, bus, slot, func, reg & ~3);

	/* Byte enables are active low, so not them first */
	PCI_CSR_WRITE_4(sc, PCI_NP_CBE, COMMAND_NP_CONF_WRITE |
	    (~(byteenables[bytes] << (reg & 3)) & 0xf0));
	PCI_CSR_WRITE_4(sc, PCI_NP_WDATA, val << ((reg & 3) * 8));

	/* check & clear PCI abort */
	data = PCI_CSR_READ_4(sc, PCI_ISR);
	if (data & ISR_PFE)
		PCI_CSR_WRITE_4(sc, PCI_ISR, ISR_PFE);
}

static int
ixppcib_route_interrupt(device_t bridge, device_t device, int pin)
{

	return (ixp425_md_route_interrupt(bridge, device, pin));
}

static device_method_t ixppcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			ixppcib_probe),
	DEVMETHOD(device_attach,		ixppcib_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		ixppcib_read_ivar),
	DEVMETHOD(bus_write_ivar,		ixppcib_write_ivar),
	DEVMETHOD(bus_setup_intr,		ixppcib_setup_intr),
	DEVMETHOD(bus_teardown_intr,		ixppcib_teardown_intr),
	DEVMETHOD(bus_alloc_resource,		ixppcib_alloc_resource),
	DEVMETHOD(bus_activate_resource,	ixppcib_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	ixppcib_deactivate_resource),
	DEVMETHOD(bus_release_resource,		ixppcib_release_resource),
	/* DEVMETHOD(bus_get_dma_tag,		ixppcib_get_dma_tag), */

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		ixppcib_maxslots),
	DEVMETHOD(pcib_read_config,		ixppcib_read_config),
	DEVMETHOD(pcib_write_config,		ixppcib_write_config),
	DEVMETHOD(pcib_route_interrupt,		ixppcib_route_interrupt),

	{0, 0},
};

static driver_t ixppcib_driver = {
	"pcib",
	ixppcib_methods,
	sizeof(struct ixppcib_softc),
};
static devclass_t ixppcib_devclass;

DRIVER_MODULE(ixppcib, ixp, ixppcib_driver, ixppcib_devclass, 0, 0);
