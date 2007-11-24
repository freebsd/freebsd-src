/*-
 * Copyright 2003 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/nexusvar.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <powerpc/ofw/ofw_pci.h>
#include <powerpc/powermac/gracklevar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "pcib_if.h"

int      badaddr(void *, size_t);  /* XXX */

/*
 * Device interface.
 */
static int		grackle_probe(device_t);
static int		grackle_attach(device_t);

/*
 * Bus interface.
 */
static int		grackle_read_ivar(device_t, device_t, int,
			    uintptr_t *);
static struct		resource * grackle_alloc_resource(device_t bus,
			    device_t child, int type, int *rid, u_long start,
			    u_long end, u_long count, u_int flags);
static int		grackle_release_resource(device_t bus, device_t child,
    			    int type, int rid, struct resource *res);
static int		grackle_activate_resource(device_t bus, device_t child,
			    int type, int rid, struct resource *res);
static int		grackle_deactivate_resource(device_t bus,
    			    device_t child, int type, int rid,
    			    struct resource *res);


/*
 * pcib interface.
 */
static int		grackle_maxslots(device_t);
static u_int32_t	grackle_read_config(device_t, u_int, u_int, u_int,
			    u_int, int);
static void		grackle_write_config(device_t, u_int, u_int, u_int,
			    u_int, u_int32_t, int);
static int		grackle_route_interrupt(device_t, device_t, int);

/*
 * Local routines.
 */
static int		grackle_enable_config(struct grackle_softc *, u_int,
			    u_int, u_int, u_int);
static void		grackle_disable_config(struct grackle_softc *);

/*
 * Driver methods.
 */
static device_method_t	grackle_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		grackle_probe),
	DEVMETHOD(device_attach,	grackle_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	grackle_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	grackle_alloc_resource),
	DEVMETHOD(bus_release_resource,	grackle_release_resource),
	DEVMETHOD(bus_activate_resource,	grackle_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	grackle_deactivate_resource),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	grackle_maxslots),
	DEVMETHOD(pcib_read_config,	grackle_read_config),
	DEVMETHOD(pcib_write_config,	grackle_write_config),
	DEVMETHOD(pcib_route_interrupt,	grackle_route_interrupt),

	{ 0, 0 }
};

static driver_t	grackle_driver = {
	"pcib",
	grackle_methods,
	sizeof(struct grackle_softc)
};

static devclass_t	grackle_devclass;

DRIVER_MODULE(grackle, nexus, grackle_driver, grackle_devclass, 0, 0);

static int
grackle_probe(device_t dev)
{
	char	*type, *compatible;

	type = nexus_get_device_type(dev);
	compatible = nexus_get_compatible(dev);

	if (type == NULL || compatible == NULL)
		return (ENXIO);

	if (strcmp(type, "pci") != 0 || strcmp(compatible, "grackle") != 0)
		return (ENXIO);

	device_set_desc(dev, "MPC106 (Grackle) Host-PCI bridge");
	return (0);
}

static int
grackle_attach(device_t dev)
{
	struct		grackle_softc *sc;
	phandle_t	node;
	u_int32_t	busrange[2];
	struct		grackle_range *rp, *io, *mem[2];
	int		nmem, i;

	node = nexus_get_node(dev);
	sc = device_get_softc(dev);

	if (OF_getprop(node, "bus-range", busrange, sizeof(busrange)) != 8)
		return (ENXIO);

	sc->sc_dev = dev;
	sc->sc_node = node;
	sc->sc_bus = busrange[0];

	/*
	 * The Grackle PCI config addr/data registers are actually in
	 * PCI space, but since they are needed to actually probe the
	 * PCI bus, use the fact that they are also available directly
	 * on the processor bus and map them
	 */
	sc->sc_addr = (vm_offset_t)pmap_mapdev(GRACKLE_ADDR, PAGE_SIZE);
	sc->sc_data = (vm_offset_t)pmap_mapdev(GRACKLE_DATA, PAGE_SIZE);

	bzero(sc->sc_range, sizeof(sc->sc_range));
	sc->sc_nrange = OF_getprop(node, "ranges", sc->sc_range,
	    sizeof(sc->sc_range));

	if (sc->sc_nrange == -1) {
		device_printf(dev, "could not get ranges\n");
		return (ENXIO);
	}

	sc->sc_range[6].pci_hi = 0;
	io = NULL;
	nmem = 0;

	for (rp = sc->sc_range; rp->pci_hi != 0; rp++) {
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
	sc->sc_io_rman.rm_descr = "Grackle PCI I/O Ports";
	sc->sc_iostart = io->pci_iospace;
	if (rman_init(&sc->sc_io_rman) != 0 ||
	    rman_manage_region(&sc->sc_io_rman, io->pci_lo,
	    io->pci_lo + io->size_lo) != 0) {
		device_printf(dev, "failed to set up io range management\n");
		return (ENXIO);
	}

	if (nmem == 0) {
		device_printf(dev, "can't find mem ranges\n");
		return (ENXIO);
	}
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "Grackle PCI Memory";
	if (rman_init(&sc->sc_mem_rman) != 0) {
		device_printf(dev,
		    "failed to init mem range resources\n");
		return (ENXIO);
	}
	for (i = 0; i < nmem; i++) {
		if (rman_manage_region(&sc->sc_mem_rman, mem[i]->pci_lo,
		    mem[i]->pci_lo + mem[i]->size_lo) != 0) {
			device_printf(dev,
			    "failed to set up memory range management\n");
			return (ENXIO);
		}
	}

	/*
	 * Write out the correct PIC interrupt values to config space
	 * of all devices on the bus.
	 */
	ofw_pci_fixup(dev, sc->sc_bus, sc->sc_node);

	device_add_child(dev, "pci", device_get_unit(dev));
	return (bus_generic_attach(dev));
}

static int
grackle_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static u_int32_t
grackle_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct		grackle_softc *sc;
	vm_offset_t	caoff;
	u_int32_t	retval = 0xffffffff;

	sc = device_get_softc(dev);
	caoff = sc->sc_data + (reg & 0x03);

	if (grackle_enable_config(sc, bus, slot, func, reg) != 0) {

		/*
		 * Config probes to non-existent devices on the
		 * secondary bus generates machine checks. Be sure
		 * to catch these.
		 */
		if (bus > 0) {
		  if (badaddr((void *)sc->sc_data, 4)) {
			  return (retval);
		  }
		}

		switch (width) {
		case 1:
			retval = (in8rb(caoff));
			break;
		case 2:
			retval = (in16rb(caoff));
			break;
		case 4:
			retval = (in32rb(caoff));
			break;
		}
	}
	grackle_disable_config(sc);

	return (retval);
}

static void
grackle_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, u_int32_t val, int width)
{
	struct		grackle_softc *sc;
	vm_offset_t	caoff;

	sc = device_get_softc(dev);
	caoff = sc->sc_data + (reg & 0x03);

	if (grackle_enable_config(sc, bus, slot, func, reg)) {
		switch (width) {
		case 1:
			out8rb(caoff, val);
			(void)in8rb(caoff);
			break;
		case 2:
			out16rb(caoff, val);
			(void)in16rb(caoff);
			break;
		case 4:
			out32rb(caoff, val);
			(void)in32rb(caoff);
			break;
		}
	}
	grackle_disable_config(sc);
}

static int
grackle_route_interrupt(device_t bus, device_t dev, int pin)
{

	return (0);
}

static int
grackle_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct	grackle_softc *sc;

	sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		*result = sc->sc_bus;
		return (0);
		break;
	}

	return (ENOENT);
}

static struct resource *
grackle_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct			grackle_softc *sc;
	struct			resource *rv;
	struct			rman *rm;
	bus_space_tag_t		bt;
	int			needactivate;

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	sc = device_get_softc(bus);

	switch (type) {
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		bt = PPC_BUS_SPACE_MEM;
		break;

	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		bt = PPC_BUS_SPACE_IO;
		break;

	case SYS_RES_IRQ:
		return (bus_alloc_resource(bus, type, rid, start, end, count,
		    flags));
		break;

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
	rman_set_bustag(rv, bt);
	rman_set_bushandle(rv, rman_get_start(rv));

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
grackle_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	if (rman_get_flags(res) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, res);
		if (error)
			return error;
	}

	return (rman_release_resource(res));
}

static int
grackle_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	struct grackle_softc *sc;
	void	*p;

	sc = device_get_softc(bus);

	if (type == SYS_RES_IRQ) {
		return (bus_activate_resource(bus, type, rid, res));
	}
	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		vm_offset_t start;

		start = (vm_offset_t)rman_get_start(res);
		/*
		 * For i/o-ports, convert the start address to the
		 * MPC106 PCI i/o window
		 */
		if (type == SYS_RES_IOPORT)
			start += sc->sc_iostart;

		if (bootverbose)
			printf("grackle mapdev: start %x, len %ld\n", start,
			    rman_get_size(res));

		p = pmap_mapdev(start, (vm_size_t)rman_get_size(res));
		if (p == NULL)
			return (ENOMEM);

		rman_set_virtual(res, p);
		rman_set_bushandle(res, (u_long)p);
	}

	return (rman_activate_resource(res));
}

static int
grackle_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	/*
	 * If this is a memory resource, unmap it.
	 */
	if ((type == SYS_RES_MEMORY) || (type == SYS_RES_IOPORT)) {
		u_int32_t psize;

		psize = rman_get_size(res);
		pmap_unmapdev((vm_offset_t)rman_get_virtual(res), psize);
	}

	return (rman_deactivate_resource(res));
}


static int
grackle_enable_config(struct grackle_softc *sc, u_int bus, u_int slot,
    u_int func, u_int reg)
{
	u_int32_t	cfgval;

	/*
	 * Unlike UniNorth, the format of the config word is the same
	 * for local (0) and remote busses.
	 */
	cfgval = (bus << 16) | (slot << 11) | (func << 8) | (reg & 0xFC)
	    | GRACKLE_CFG_ENABLE;

	out32rb(sc->sc_addr, cfgval);
	(void) in32rb(sc->sc_addr);

	return (1);
}

static void
grackle_disable_config(struct grackle_softc *sc)
{
	/*
	 * Clear the GRACKLE_CFG_ENABLE bit to prevent stray
	 * accesses from causing config cycles
	 */
	out32rb(sc->sc_addr, 0);
}

/*
 * Driver to swallow Grackle host bridges from the PCI bus side.
 */
static int
grackle_hb_probe(device_t dev)
{

	if (pci_get_devid(dev) == 0x00021057) {
		device_set_desc(dev, "Grackle Host to PCI bridge");
		device_quiet(dev);
		return (0);
	}

	return (ENXIO);
}

static int
grackle_hb_attach(device_t dev)
{

	return (0);
}

static device_method_t grackle_hb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         grackle_hb_probe),
	DEVMETHOD(device_attach,        grackle_hb_attach),

	{ 0, 0 }
};

static driver_t grackle_hb_driver = {
	"grackle_hb",
	grackle_hb_methods,
	1,
};
static devclass_t grackle_hb_devclass;

DRIVER_MODULE(grackle_hb, pci, grackle_hb_driver, grackle_hb_devclass, 0, 0);
