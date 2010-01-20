/*-
 * Copyright (C) 2008 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
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

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <powerpc/powermac/cpchtvar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "pcib_if.h"

#include "opt_isa.h"

#ifdef DEV_ISA
#include <isa/isavar.h>
#endif

static MALLOC_DEFINE(M_CPCHT, "cpcht", "CPC HT device information");

/*
 * HT Driver methods.
 */
static int		cpcht_probe(device_t);
static int		cpcht_attach(device_t);
static ofw_bus_get_devinfo_t cpcht_get_devinfo;


static device_method_t	cpcht_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cpcht_probe),
	DEVMETHOD(device_attach,	cpcht_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,bus_generic_activate_resource),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,  cpcht_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,   ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,    ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,     ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,     ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,     ofw_bus_gen_get_type),

	{ 0, 0 }
};

static driver_t	cpcht_driver = {
	"cpcht",
	cpcht_methods,
	0
};

static devclass_t	cpcht_devclass;

DRIVER_MODULE(cpcht, nexus, cpcht_driver, cpcht_devclass, 0, 0);

static int 
cpcht_probe(device_t dev) 
{
	const char	*type, *compatible;

	type = ofw_bus_get_type(dev);
	compatible = ofw_bus_get_compat(dev);

	if (type == NULL || compatible == NULL)
		return (ENXIO);

	if (strcmp(type, "ht") != 0)
		return (ENXIO);

	if (strcmp(compatible, "u3-ht") == 0) {
		device_set_desc(dev, "IBM CPC925 HyperTransport Tunnel");
		return (0);
	} else if (strcmp(compatible,"u4-ht") == 0) {
		device_set_desc(dev, "IBM CPC945 HyperTransport Tunnel");
		return (0);
	}

	return (ENXIO);
}

static int 
cpcht_attach(device_t dev) 
{
	phandle_t root, child;
	device_t cdev;
	struct ofw_bus_devinfo *dinfo;
	u_int32_t reg[6];

	root = ofw_bus_get_node(dev);

	if (OF_getprop(root, "reg", reg, sizeof(reg)) < 8)
		return (ENXIO);

	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		dinfo = malloc(sizeof(*dinfo), M_CPCHT, M_WAITOK | M_ZERO);

                if (ofw_bus_gen_setup_devinfo(dinfo, child) != 0) {
                        free(dinfo, M_CPCHT);
                        continue;
                }
                cdev = device_add_child(dev, NULL, -1);
                if (cdev == NULL) {
                        device_printf(dev, "<%s>: device_add_child failed\n",
                            dinfo->obd_name);
                        ofw_bus_gen_destroy_devinfo(dinfo);
                        free(dinfo, M_CPCHT);
                        continue;
                }
		device_set_ivars(cdev, dinfo);
	}

	return (bus_generic_attach(dev));
}

static const struct ofw_bus_devinfo *
cpcht_get_devinfo(device_t dev, device_t child) 
{
	return (device_get_ivars(child));	
}

#ifdef DEV_ISA

/*
 * CPC ISA Device interface.
 */
static int		cpcisa_probe(device_t);

/*
 * Driver methods.
 */
static device_method_t	cpcisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cpcisa_probe),
	DEVMETHOD(device_attach,	isab_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,bus_generic_activate_resource),

	{0,0}
};

static driver_t	cpcisa_driver = {
	"isab",
	cpcisa_methods,
	0
};

DRIVER_MODULE(cpcisa, cpcht, cpcisa_driver, isab_devclass, 0, 0);

static int
cpcisa_probe(device_t dev)
{
	const char	*type;

	type = ofw_bus_get_type(dev);

	if (type == NULL)
		return (ENXIO);

	if (strcmp(type, "isa") != 0)
		return (ENXIO);

	device_set_desc(dev, "HyperTransport-ISA bridge");
	
	return (0);
}

#endif /* DEV_ISA */

/*
 * CPC PCI Device interface.
 */
static int		cpcpci_probe(device_t);
static int		cpcpci_attach(device_t);

/*
 * Bus interface.
 */
static int		cpcpci_read_ivar(device_t, device_t, int,
			    uintptr_t *);
static struct		resource * cpcpci_alloc_resource(device_t bus,
			    device_t child, int type, int *rid, u_long start,
			    u_long end, u_long count, u_int flags);
static int		cpcpci_activate_resource(device_t bus, device_t child,
			    int type, int rid, struct resource *res);

/*
 * pcib interface.
 */
static int		cpcpci_maxslots(device_t);
static u_int32_t	cpcpci_read_config(device_t, u_int, u_int, u_int,
			    u_int, int);
static void		cpcpci_write_config(device_t, u_int, u_int, u_int,
			    u_int, u_int32_t, int);
static int		cpcpci_route_interrupt(device_t, device_t, int);

/*
 * ofw_bus interface
 */

static phandle_t	cpcpci_get_node(device_t bus, device_t child);

/*
 * Driver methods.
 */
static device_method_t	cpcpci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cpcpci_probe),
	DEVMETHOD(device_attach,	cpcpci_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	cpcpci_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	cpcpci_alloc_resource),
	DEVMETHOD(bus_activate_resource,	cpcpci_activate_resource),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	cpcpci_maxslots),
	DEVMETHOD(pcib_read_config,	cpcpci_read_config),
	DEVMETHOD(pcib_write_config,	cpcpci_write_config),
	DEVMETHOD(pcib_route_interrupt,	cpcpci_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,     cpcpci_get_node),
	{ 0, 0 }
};

static driver_t	cpcpci_driver = {
	"pcib",
	cpcpci_methods,
	sizeof(struct cpcpci_softc)
};

static devclass_t	cpcpci_devclass;

DRIVER_MODULE(cpcpci, cpcht, cpcpci_driver, cpcpci_devclass, 0, 0);

static int
cpcpci_probe(device_t dev)
{
	const char	*type;

	type = ofw_bus_get_type(dev);

	if (type == NULL)
		return (ENXIO);

	if (strcmp(type, "pci") != 0)
		return (ENXIO);

	device_set_desc(dev, "HyperTransport-PCI bridge");
	
	return (0);
}

static int
cpcpci_attach(device_t dev)
{
	struct		cpcpci_softc *sc;
	phandle_t	node;
	u_int32_t	reg[2], busrange[2], config_base;
	struct		cpcpci_range *rp, *io, *mem[2];
	struct		cpcpci_range fakeio;
	int		nmem, i;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);

	if (OF_getprop(OF_parent(node), "reg", reg, sizeof(reg)) < 8)
		return (ENXIO);

	if (OF_getprop(node, "bus-range", busrange, sizeof(busrange)) != 8)
		return (ENXIO);

	sc->sc_dev = dev;
	sc->sc_node = node;
	sc->sc_bus = busrange[0];
	config_base = reg[1];
	if (sc->sc_bus)
		config_base += 0x01000000UL + (sc->sc_bus << 16);
	sc->sc_data = (vm_offset_t)pmap_mapdev(config_base, PAGE_SIZE << 4);

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
		/*
		 * On at least some machines, the I/O port range is
		 * not exported in the OF device tree. So hardcode it.
		 */

		fakeio.host_lo = 0;
		fakeio.pci_lo = reg[1];
		fakeio.size_lo = 0x00400000;
		if (sc->sc_bus)
			fakeio.pci_lo += 0x02000000UL + (sc->sc_bus << 14);
		io = &fakeio;
	}
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "CPC 9xx PCI I/O Ports";
	sc->sc_iostart = io->host_lo;
	if (rman_init(&sc->sc_io_rman) != 0 ||
	    rman_manage_region(&sc->sc_io_rman, io->pci_lo,
	    io->pci_lo + io->size_lo - 1) != 0) {
		device_printf(dev, "failed to set up io range management\n");
		return (ENXIO);
	}

	if (nmem == 0) {
		device_printf(dev, "can't find mem ranges\n");
		return (ENXIO);
	}
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "CPC 9xx PCI Memory";
	if (rman_init(&sc->sc_mem_rman) != 0) {
		device_printf(dev,
		    "failed to init mem range resources\n");
		return (ENXIO);
	}
	for (i = 0; i < nmem; i++) {
		if (rman_manage_region(&sc->sc_mem_rman, mem[i]->pci_lo,
		    mem[i]->pci_lo + mem[i]->size_lo - 1) != 0) {
			device_printf(dev,
			    "failed to set up memory range management\n");
			return (ENXIO);
		}
	}

	ofw_bus_setup_iinfo(node, &sc->sc_pci_iinfo, sizeof(cell_t));

	device_add_child(dev, "pci", device_get_unit(dev));

	return (bus_generic_attach(dev));
}

static int
cpcpci_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static u_int32_t
cpcpci_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct		cpcpci_softc *sc;
	vm_offset_t	caoff;

	sc = device_get_softc(dev);
	caoff = sc->sc_data + 
		(((((slot & 0x1f) << 3) | (func & 0x07)) << 8) | reg);

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

	return (0xffffffff);
}

static void
cpcpci_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, u_int32_t val, int width)
{
	struct		cpcpci_softc *sc;
	vm_offset_t	caoff;

	sc = device_get_softc(dev);
	caoff = sc->sc_data + 
		(((((slot & 0x1f) << 3) | (func & 0x07)) << 8) | reg);

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

static int
cpcpci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct	cpcpci_softc *sc;

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
cpcpci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct			cpcpci_softc *sc;
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
		if (rm == NULL)
			return (NULL);
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
cpcpci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	void	*p;
	struct	cpcpci_softc *sc;

	sc = device_get_softc(bus);

	if (type == SYS_RES_IRQ)
		return (bus_activate_resource(bus, type, rid, res));

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		vm_offset_t start;

		start = (vm_offset_t)rman_get_start(res);
		/*
		 * For i/o-ports, convert the start address to the
		 * CPC PCI i/o window
		 */
		if (type == SYS_RES_IOPORT)
			start += sc->sc_iostart;

		if (bootverbose)
			printf("cpcpci mapdev: start %x, len %ld\n", start,
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

static phandle_t
cpcpci_get_node(device_t bus, device_t dev)
{
	struct cpcpci_softc *sc;

	sc = device_get_softc(bus);
	/* We only have one child, the PCI bus, which needs our own node. */
	return (sc->sc_node);
}

static int
cpcpci_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct cpcpci_softc *sc;
	struct ofw_pci_register reg;
	uint32_t pintr, mintr;
	uint8_t maskbuf[sizeof(reg) + sizeof(pintr)];

	sc = device_get_softc(bus);
	pintr = pin;
	if (ofw_bus_lookup_imap(ofw_bus_get_node(dev), &sc->sc_pci_iinfo, &reg,
	    sizeof(reg), &pintr, sizeof(pintr), &mintr, sizeof(mintr), maskbuf))
		return (mintr);

	/* Maybe it's a real interrupt, not an intpin */
	if (pin > 4)
		return (pin);

	device_printf(bus, "could not route pin %d for device %d.%d\n",
	    pin, pci_get_slot(dev), pci_get_function(dev));
	return (PCI_INVALID_IRQ);
}

