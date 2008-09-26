/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Child driver for PCI host bridge core.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/malloc.h>
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/pmap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

#include <dev/siba/sibavar.h>
#include <dev/siba/sibareg.h>
#include <dev/siba/siba_ids.h>
#include <dev/siba/siba_pcibvar.h>

#ifndef MIPS_MEM_RID
#define MIPS_MEM_RID 0x20
#endif

#define SBPCI_SLOTMAX 15

#define SBPCI_READ_4(sc, reg)					\
	bus_space_write_4((sc)->sc_bt, (sc)->sc_bh, (reg))

#define SBPCI_WRITE_4(sc, reg, val)					\
	bus_space_write_4((sc)->sc_bt, (sc)->sc_bh, (reg), (val))

/*
 * PCI Configuration space window (64MB).
 * contained in SBTOPCI1 window.
 */
#define SBPCI_CFGBASE			0x0C000000
#define SBPCI_CFGSIZE			0x01000000

#define SBPCI_SBTOPCI0 0x100
#define SBPCI_SBTOPCI1 0x104
#define SBPCI_SBTOPCI2 0x108

/*
 * TODO: implement type 1 config space access (ie beyond bus 0)
 * we may need to tweak the windows to do this
 * TODO: interrupt routing.
 * TODO: fully implement bus allocation.
 * TODO: implement resource managers.
 * TODO: code cleanup.
 */

static int	siba_pcib_activate_resource(device_t, device_t, int,
		    int, struct resource *);
static struct resource *
		siba_pcib_alloc_resource(device_t, device_t, int, int *,
		    u_long , u_long, u_long, u_int);
static int	siba_pcib_attach(device_t);
static int	siba_pcib_deactivate_resource(device_t, device_t, int,
		    int, struct resource *);
static int	siba_pcib_maxslots(device_t);
static int	siba_pcib_probe(device_t);
static u_int32_t
		siba_pcib_read_config(device_t, u_int, u_int, u_int, u_int,
		    int);
static int	siba_pcib_read_ivar(device_t, device_t, int, uintptr_t *);
static int	siba_pcib_release_resource(device_t, device_t, int, int,
		    struct resource *);
static int	siba_pcib_route_interrupt(device_t, device_t, int);
static int	siba_pcib_setup_intr(device_t, device_t, struct resource *,
		    int, driver_filter_t *, driver_intr_t *, void *, void **);
static int	siba_pcib_teardown_intr(device_t, device_t, struct resource *,
		    void *);
static void	siba_pcib_write_config(device_t, u_int, u_int, u_int, u_int,
		    u_int32_t, int);
static int	siba_pcib_write_ivar(device_t, device_t, int, uintptr_t);

static int
siba_pcib_probe(device_t dev)
{

	/* TODO: support earlier cores. */
	/* TODO: Check if PCI host mode is enabled in the SPROM. */
	if (siba_get_vendor(dev) == SIBA_VID_BROADCOM &&
	    siba_get_device(dev) == SIBA_DEVID_PCI) {
		device_set_desc(dev, "SiBa-to-PCI host bridge");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

//extern int rman_debug;

static int
siba_pcib_attach(device_t dev)
{
	struct siba_pcib_softc *sc = device_get_softc(dev);
	int rid;

	/*
	 * Allocate the resources which the parent bus has already
	 * determined for us.
	 */
	rid = MIPS_MEM_RID;	/* XXX */
	//rman_debug = 1;
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "unable to allocate memory\n");
		return (ENXIO);
	}

	sc->sc_bt = rman_get_bustag(sc->sc_mem);
	sc->sc_bh = rman_get_bushandle(sc->sc_mem);

	device_printf(dev, "bridge registers addr 0x%08x vaddr %p\n",
	    sc->sc_bh, rman_get_virtual(sc->sc_mem));

	SBPCI_WRITE_4(sc, 0x0000, 0x05);
	SBPCI_WRITE_4(sc, 0x0000, 0x0D);
	DELAY(150);
	SBPCI_WRITE_4(sc, 0x0000, 0x0F);
	SBPCI_WRITE_4(sc, 0x0010, 0x01);
	DELAY(1);

	bus_space_handle_t sc_cfg_hand;
	int error;

	/*
	 * XXX this doesn't actually do anything on mips; however... should
	 * we not be mapping to KSEG1? we need to wire down the range.
	 */
	error = bus_space_map(sc->sc_bt, SBPCI_CFGBASE, SBPCI_CFGSIZE,
	    0, &sc_cfg_hand);
	if (error) {
		device_printf(dev, "cannot map PCI configuration space\n");
		return (ENXIO);
	}
	device_printf(dev, "mapped pci config space at 0x%08x\n", sc_cfg_hand);

	/*
	 * Setup configuration, io, and dma space windows.
	 * XXX we need to be able to do type 1 too.
	 * we probably don't need to be able to do i/o cycles.
	 */
	SBPCI_WRITE_4(sc, SBPCI_SBTOPCI0, 1);	/* I/O read/write window */
	SBPCI_WRITE_4(sc, SBPCI_SBTOPCI1, 2);	/* type 0 configuration only */
	SBPCI_WRITE_4(sc, SBPCI_SBTOPCI2, 1 << 30); /* memory only */
	DELAY(500);

	/* XXX resource managers */

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

/* bus functions */

static int
siba_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct siba_pcib_softc *sc;

	sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_BUS:
		*result = sc->sc_bus;
		return (0);
	}

	return (ENOENT);
}

static int
siba_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct siba_pcib_softc *sc;

	sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_bus = value;
		return (0);
	}

	return (ENOENT);
}

static int
siba_pcib_setup_intr(device_t dev, device_t child, struct resource *ires,
    int flags, driver_filter_t *filt, driver_intr_t *intr, void *arg,
    void **cookiep)
{

	return (BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    filt, intr, arg, cookiep));
}

static int
siba_pcib_teardown_intr(device_t dev, device_t child, struct resource *vec,
     void *cookie)
{

	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, vec, cookie));
}

static struct resource *
siba_pcib_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
#if 1

	//device_printf(bus, "%s: not yet implemented\n", __func__);
	return (NULL);
#else
	bus_space_tag_t tag;
	struct siba_pcib_softc *sc = device_get_softc(bus);
	struct rman *rmanp;
	struct resource *rv;

	tag = 0;
	rv = NULL;
	switch (type) {
	case SYS_RES_IRQ:
		rmanp = &sc->sc_irq_rman;
		break;

	case SYS_RES_MEMORY:
		rmanp = &sc->sc_mem_rman;
		tag = &sc->sc_pci_memt;
		break;

	default:
		return (rv);
	}

	rv = rman_reserve_resource(rmanp, start, end, count, flags, child);
	if (rv != NULL) {
		rman_set_rid(rv, *rid);
		if (type == SYS_RES_MEMORY) {
#if 0
			rman_set_bustag(rv, tag);
			rman_set_bushandle(rv, rman_get_bushandle(sc->sc_mem) +
			    (rman_get_start(rv) - IXP425_PCI_MEM_HWBASE));
#endif
		}
	}

	return (rv);
#endif
}

static int
siba_pcib_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	device_printf(bus, "%s: not yet implemented\n", __func__);
	device_printf(bus, "%s called activate_resource\n",
	    device_get_nameunit(child));
	return (ENXIO);
}

static int
siba_pcib_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	device_printf(bus, "%s: not yet implemented\n", __func__);
	device_printf(bus, "%s called deactivate_resource\n",
	    device_get_nameunit(child));
	return (ENXIO);
}

static int
siba_pcib_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	device_printf(bus, "%s: not yet implemented\n", __func__);
	device_printf(bus, "%s called release_resource\n",
	    device_get_nameunit(child));
	return (ENXIO);
}

/* pcib interface functions */

static int
siba_pcib_maxslots(device_t dev)
{

	return (SBPCI_SLOTMAX);
}

/*
 * This needs hacking and fixery. It is currently broke and hangs.
 * Debugging it will be tricky; there seems to be no way to enable
 * a target abort which would cause a nice target abort.
 * Look at linux again?
 */
static u_int32_t
siba_pcib_read_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct siba_pcib_softc *sc = device_get_softc(dev);
	bus_addr_t cfgaddr;
	uint32_t cfgtag;
	uint32_t val;

	/* XXX anything higher than slot 2 currently seems to hang the bus.
	 * not sure why this is; look at linux again
	 */
	if (bus != 0 || slot > 2) {
		printf("%s: bad b/s/f %d/%d/%d\n", __func__, bus, slot, func);
		return 0xffffffff;	// XXX
	}

	device_printf(dev, "requested %d bytes from b/s/f %d/%d/%d reg %d\n",
	    bytes, bus, slot, func, reg);

	/*
	 * The configuration tag on the broadcom is weird.
	 */
	SBPCI_WRITE_4(sc, SBPCI_SBTOPCI1, 2);	/* XXX again??? */
	cfgtag = ((1 << slot) << 16) | (func << 8);
	cfgaddr = SBPCI_CFGBASE | cfgtag | (reg & ~3);

	/* cfg space i/o is always 32 bits on this bridge */
	printf("reading 4 bytes from %08x\n", cfgaddr);
	val = *(volatile uint32_t *)MIPS_PHYS_TO_KSEG1(cfgaddr); /* XXX MIPS */

	val = bswap32(val);	/* XXX seems to be needed for now */

	/* swizzle and return what was asked for */
	val &= 0xffffffff >> ((4 - bytes) * 8);

	return (val);
}

static void
siba_pcib_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, u_int32_t val, int bytes)
{

	/* write to pci configuration space */
	//device_printf(dev, "%s: not yet implemented\n", __func__);
}

static int
siba_pcib_route_interrupt(device_t bridge, device_t device, int pin)
{

	//device_printf(bridge, "%s: not yet implemented\n", __func__);
	return (-1);
}

static device_method_t siba_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	siba_pcib_attach),
	DEVMETHOD(device_probe,		siba_pcib_probe),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	siba_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	siba_pcib_write_ivar),
	DEVMETHOD(bus_setup_intr,	siba_pcib_setup_intr),
	DEVMETHOD(bus_teardown_intr,	siba_pcib_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	siba_pcib_alloc_resource),
	DEVMETHOD(bus_activate_resource,	siba_pcib_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	siba_pcib_deactivate_resource),
	DEVMETHOD(bus_release_resource,	siba_pcib_release_resource),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	siba_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	siba_pcib_read_config),
	DEVMETHOD(pcib_write_config,	siba_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	siba_pcib_route_interrupt),

	{0, 0},
};

static driver_t siba_pcib_driver = {
	"pcib",
	siba_pcib_methods,
	sizeof(struct siba_softc),
};
static devclass_t siba_pcib_devclass;

DRIVER_MODULE(siba_pcib, siba, siba_pcib_driver, siba_pcib_devclass, 0, 0);
