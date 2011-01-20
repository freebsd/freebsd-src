/*-
 * Copyright (C) 2002 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <powerpc/powermac/uninorthvar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "pcib_if.h"

#define	UNINORTH_DEBUG	0

/*
 * Device interface.
 */
static int		uninorth_probe(device_t);
static int		uninorth_attach(device_t);

/*
 * Bus interface.
 */
static int		uninorth_read_ivar(device_t, device_t, int,
			    uintptr_t *);
static struct		resource * uninorth_alloc_resource(device_t bus,
			    device_t child, int type, int *rid, u_long start,
			    u_long end, u_long count, u_int flags);
static int		uninorth_activate_resource(device_t bus, device_t child,
			    int type, int rid, struct resource *res);

/*
 * pcib interface.
 */
static int		uninorth_maxslots(device_t);
static u_int32_t	uninorth_read_config(device_t, u_int, u_int, u_int,
			    u_int, int);
static void		uninorth_write_config(device_t, u_int, u_int, u_int,
			    u_int, u_int32_t, int);
static int		uninorth_route_interrupt(device_t, device_t, int);

/*
 * OFW Bus interface
 */

static phandle_t	 uninorth_get_node(device_t bus, device_t dev);

/*
 * Local routines.
 */
static int		uninorth_enable_config(struct uninorth_softc *, u_int,
			    u_int, u_int, u_int);

/*
 * Driver methods.
 */
static device_method_t	uninorth_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uninorth_probe),
	DEVMETHOD(device_attach,	uninorth_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	uninorth_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	uninorth_alloc_resource),
	DEVMETHOD(bus_activate_resource,	uninorth_activate_resource),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	uninorth_maxslots),
	DEVMETHOD(pcib_read_config,	uninorth_read_config),
	DEVMETHOD(pcib_write_config,	uninorth_write_config),
	DEVMETHOD(pcib_route_interrupt,	uninorth_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,     uninorth_get_node),

	{ 0, 0 }
};

static driver_t	uninorth_driver = {
	"pcib",
	uninorth_methods,
	sizeof(struct uninorth_softc)
};

static devclass_t	uninorth_devclass;

DRIVER_MODULE(uninorth, nexus, uninorth_driver, uninorth_devclass, 0, 0);

static int
uninorth_probe(device_t dev)
{
	const char	*type, *compatible;

	type = ofw_bus_get_type(dev);
	compatible = ofw_bus_get_compat(dev);

	if (type == NULL || compatible == NULL)
		return (ENXIO);

	if (strcmp(type, "pci") != 0)
		return (ENXIO);

	if (strcmp(compatible, "uni-north") == 0) {
		device_set_desc(dev, "Apple UniNorth Host-PCI bridge");
		return (0);
	} else if (strcmp(compatible, "u3-agp") == 0) {
		device_set_desc(dev, "Apple U3 Host-AGP bridge");
		return (0);
	} else if (strcmp(compatible, "u4-pcie") == 0) {
		device_set_desc(dev, "IBM CPC945 PCI Express Root");
		return (0);
	}
	
	return (ENXIO);
}

static int
uninorth_attach(device_t dev)
{
	struct		uninorth_softc *sc;
	const char	*compatible;
	phandle_t	node;
	u_int32_t	reg[3], busrange[2];
	struct		uninorth_range *rp, *io, *mem[2];
	int		nmem, i, error;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);

	if (OF_getprop(node, "reg", reg, sizeof(reg)) < 8)
		return (ENXIO);

	if (OF_getprop(node, "bus-range", busrange, sizeof(busrange)) != 8)
		return (ENXIO);

	sc->sc_ver = 0;
	compatible = ofw_bus_get_compat(dev);
	if (strcmp(compatible, "u3-agp") == 0)
		sc->sc_ver = 3;
	if (strcmp(compatible, "u4-pcie") == 0)
		sc->sc_ver = 4;

	sc->sc_dev = dev;
	sc->sc_node = node;
	if (sc->sc_ver >= 3) {
	   sc->sc_addr = (vm_offset_t)pmap_mapdev(reg[1] + 0x800000, PAGE_SIZE);
	   sc->sc_data = (vm_offset_t)pmap_mapdev(reg[1] + 0xc00000, PAGE_SIZE);
	} else {
	   sc->sc_addr = (vm_offset_t)pmap_mapdev(reg[0] + 0x800000, PAGE_SIZE);
	   sc->sc_data = (vm_offset_t)pmap_mapdev(reg[0] + 0xc00000, PAGE_SIZE);
	}
	sc->sc_bus = busrange[0];

	bzero(sc->sc_range, sizeof(sc->sc_range));
	if (sc->sc_ver >= 3) {
		/*
		 * On Apple U3 systems, we have an otherwise standard
		 * Uninorth controller driving AGP. The one difference
		 * is that it uses a new PCI ranges format, so do the
		 * translation.
		 */

		struct uninorth_range64 range64[6];
		bzero(range64, sizeof(range64));

		sc->sc_nrange = OF_getprop(node, "ranges", range64,
		    sizeof(range64));
		for (i = 0; range64[i].pci_hi != 0; i++) {
			sc->sc_range[i].pci_hi = range64[i].pci_hi;
			sc->sc_range[i].pci_mid = range64[i].pci_mid;
			sc->sc_range[i].pci_lo = range64[i].pci_lo;
			sc->sc_range[i].host = range64[i].host_lo;
			sc->sc_range[i].size_hi = range64[i].size_hi;
			sc->sc_range[i].size_lo = range64[i].size_lo;
		}
	} else {
		sc->sc_nrange = OF_getprop(node, "ranges", sc->sc_range,
		    sizeof(sc->sc_range));
	}

	if (sc->sc_nrange == -1) {
		device_printf(dev, "could not get ranges\n");
		return (ENXIO);
	}

	sc->sc_nrange /= sizeof(sc->sc_range[0]);

	sc->sc_range[6].pci_hi = 0;
	io = NULL;
	nmem = 0;

	for (rp = sc->sc_range; rp < sc->sc_range + sc->sc_nrange &&
	       rp->pci_hi != 0; rp++) {
		switch (rp->pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
		case OFW_PCI_PHYS_HI_SPACE_CONFIG:
			break;
		case OFW_PCI_PHYS_HI_SPACE_IO:
			io = rp;
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM32:
			mem[nmem] = rp;
			nmem++;
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM64:
			break;
		}
	}

	if (io == NULL) {
		device_printf(dev, "can't find io range\n");
		return (ENXIO);
	}
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "UniNorth PCI I/O Ports";
	sc->sc_iostart = io->host;
	if (rman_init(&sc->sc_io_rman) != 0 ||
	    rman_manage_region(&sc->sc_io_rman, io->pci_lo,
	    io->pci_lo + io->size_lo - 1) != 0) {
		panic("uninorth_attach: failed to set up I/O rman");
	}

	if (nmem == 0) {
		device_printf(dev, "can't find mem ranges\n");
		return (ENXIO);
	}
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "UniNorth PCI Memory";
	error = rman_init(&sc->sc_mem_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}
	for (i = 0; i < nmem; i++) {
		error = rman_manage_region(&sc->sc_mem_rman, mem[i]->pci_lo,
		    mem[i]->pci_lo + mem[i]->size_lo - 1);
		if (error) {
			device_printf(dev,
			    "rman_manage_region() failed. error = %d\n", error);
			return (error);
		}
	}

	ofw_bus_setup_iinfo(node, &sc->sc_pci_iinfo, sizeof(cell_t));

	device_add_child(dev, "pci", device_get_unit(dev));
	return (bus_generic_attach(dev));
}

static int
uninorth_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static u_int32_t
uninorth_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct		uninorth_softc *sc;
	vm_offset_t	caoff;

	sc = device_get_softc(dev);
	caoff = sc->sc_data + (reg & 0x07);

	if (uninorth_enable_config(sc, bus, slot, func, reg) != 0) {
		switch (width) {
		case 1: 
			return (in8rb(caoff));
			break;
		case 2:
			return (in16rb(caoff));
			break;
		case 4:
			return (in32rb(caoff));
			break;
		}
	}

	return (0xffffffff);
}

static void
uninorth_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, u_int32_t val, int width)
{
	struct		uninorth_softc *sc;
	vm_offset_t	caoff;

	sc = device_get_softc(dev);
	caoff = sc->sc_data + (reg & 0x07);

	if (uninorth_enable_config(sc, bus, slot, func, reg)) {
		switch (width) {
		case 1:
			out8rb(caoff, val);
			break;
		case 2:
			out16rb(caoff, val);
			break;
		case 4:
			out32rb(caoff, val);
			break;
		}
	}
}

static int
uninorth_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct uninorth_softc *sc;
	struct ofw_pci_register reg;
	uint32_t pintr, mintr;
	phandle_t iparent;
	uint8_t maskbuf[sizeof(reg) + sizeof(pintr)];

	sc = device_get_softc(bus);
	pintr = pin;
	if (ofw_bus_lookup_imap(ofw_bus_get_node(dev), &sc->sc_pci_iinfo, &reg,
	    sizeof(reg), &pintr, sizeof(pintr), &mintr, sizeof(mintr),
	    &iparent, maskbuf))
		return (INTR_VEC(iparent, mintr));

	/* Maybe it's a real interrupt, not an intpin */
	if (pin > 4)
		return (pin);

	device_printf(bus, "could not route pin %d for device %d.%d\n",
	    pin, pci_get_slot(dev), pci_get_function(dev));
	return (PCI_INVALID_IRQ);
}

static int
uninorth_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct	uninorth_softc *sc;

	sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = device_get_unit(dev);
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_bus;
		return (0);
	}

	return (ENOENT);
}

static struct resource *
uninorth_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct			uninorth_softc *sc;
	struct			resource *rv;
	struct			rman *rm;
	int			needactivate;

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	sc = device_get_softc(bus);

	switch (type) {
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;

	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		break;

	case SYS_RES_IRQ:
		return (bus_alloc_resource(bus, type, rid, start, end, count,
		    flags));

	default:
		device_printf(bus, "unknown resource request from %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL) {
		device_printf(bus, "failed to reserve resource for %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	rman_set_rid(rv, *rid);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv) != 0) {
			device_printf(bus,
			    "failed to activate resource for %s\n",
			    device_get_nameunit(child));
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
uninorth_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	void	*p;
	struct	uninorth_softc *sc;

	sc = device_get_softc(bus);

	if (type == SYS_RES_IRQ)
		return (bus_activate_resource(bus, type, rid, res));

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		vm_offset_t start;

		start = (vm_offset_t)rman_get_start(res);
		/*
		 * For i/o-ports, convert the start address to the
		 * uninorth PCI i/o window
		 */
		if (type == SYS_RES_IOPORT)
			start += sc->sc_iostart;

		if (bootverbose)
			printf("uninorth mapdev: start %zx, len %ld\n", start,
			    rman_get_size(res));

		p = pmap_mapdev(start, (vm_size_t)rman_get_size(res));
		if (p == NULL)
			return (ENOMEM);
		rman_set_virtual(res, p);
		rman_set_bustag(res, &bs_le_tag);
		rman_set_bushandle(res, (u_long)p);
	}

	return (rman_activate_resource(res));
}

static int
uninorth_enable_config(struct uninorth_softc *sc, u_int bus, u_int slot,
    u_int func, u_int reg)
{
	uint32_t	cfgval;
	uint32_t	pass;

	if (resource_int_value(device_get_name(sc->sc_dev),
	        device_get_unit(sc->sc_dev), "skipslot", &pass) == 0) {
		if (pass == slot)
			return (0);
	}

	/*
	 * Issue type 0 configuration space accesses for the root bus.
	 *
	 * NOTE: On U4, issue only type 1 accesses. There is a secret
	 * PCI Express <-> PCI Express bridge not present in the device tree,
	 * and we need to route all of our configuration space through it.
	 */
	if (sc->sc_bus == bus && sc->sc_ver < 4) {
		/*
		 * No slots less than 11 on the primary bus on U3 and lower
		 */
		if (slot < 11)
			return (0);

		cfgval = (1 << slot) | (func << 8) | (reg & 0xfc);
	} else {
		cfgval = (bus << 16) | (slot << 11) | (func << 8) |
		    (reg & 0xfc) | 1;
	}

	/* Set extended register bits on U4 */
	if (sc->sc_ver == 4)
		cfgval |= (reg >> 8) << 28;

	do {
		out32rb(sc->sc_addr, cfgval);
	} while (in32rb(sc->sc_addr) != cfgval);

	return (1);
}

static phandle_t
uninorth_get_node(device_t bus, device_t dev)
{
	struct uninorth_softc *sc;

	sc = device_get_softc(bus);
	/* We only have one child, the PCI bus, which needs our own node. */

	return sc->sc_node;
}

