/*
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001 Thomas Moestl <tmm@FreeBSD.org>
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
 *	from: NetBSD: ebus.c,v 1.26 2001/09/10 16:27:53 eeh Exp
 *
 * $FreeBSD$
 */

#include "opt_ebus.h"

/*
 * UltraSPARC 5 and beyond ebus support.
 *
 * note that this driver is not complete:
 *	- ebus2 dma code is completely unwritten
 *	- interrupt establish is written and appears to work
 *	- bus map code is written and appears to work
 * XXX: This is PCI specific, however, there exist SBus-to-EBus bridges...
 * XXX: The EBus was designed to allow easy adaption of ISA devices to it - a
 * compatability layer for ISA devices might be nice, although probably not
 * easily possible because of some cruft (like in[bwl]/out[bwl] and friends).
 * Additionally, the existing ISA code is limited to one ISA bus, however,
 * there are machines with both ISA and EBus.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/ofw_bus.h>
#include <machine/resource.h>

#include <ofw/openfirm.h>
#include <ofw/ofw_pci.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <sparc64/pci/ofw_pci.h>

/*
 * The register, ranges and interrupt map properties are identical to the ISA
 * ones.
 */
#include <sparc64/isa/ofw_isa.h>
#include <sparc64/ebus/ebusvar.h>

#ifdef EBUS_DEBUG
#define	EDB_PROM	0x01
#define EDB_CHILD	0x02
#define	EDB_INTRMAP	0x04
#define EDB_BUSMAP	0x08
#define EDB_BUSDMA	0x10
#define EDB_INTR	0x20
int ebus_debug = 0xff;
#define DPRINTF(l, s)   do { if (ebus_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

struct ebus_devinfo {
	char			*edi_name;	/* PROM name */
	char			*edi_compat;
	phandle_t		edi_node;	/* PROM node */

	struct resource_list	edi_rl;
};

struct ebus_softc {
	phandle_t		sc_node;

	struct isa_ranges	*sc_range;

	struct ofw_pci_register	*sc_reg;

	int			sc_imap_type;

	struct isa_imap		*sc_ebus_imap;
	struct isa_imap_msk	sc_ebus_imapmsk;

	struct ofw_pci_imap	*sc_pci_imap;
	struct ofw_pci_imap_msk	sc_pci_imapmsk;

	int			sc_nrange;
	int			sc_nreg;
	int			sc_nimap;
};

static int ebus_probe(device_t);
static int ebus_print_child(device_t, device_t);
static void ebus_probe_nomatch(device_t, device_t);
static int ebus_read_ivar(device_t, device_t, int, uintptr_t *);
static int ebus_write_ivar(device_t, device_t, int, uintptr_t);
static struct resource *ebus_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static struct resource_list *ebus_get_resource_list(device_t, device_t);

static struct ebus_devinfo *ebus_setup_dinfo(struct ebus_softc *,
    phandle_t, char *);
static void ebus_destroy_dinfo(struct ebus_devinfo *);
static int ebus_print_res(struct ebus_devinfo *);
static int ebus_map_intr(struct ebus_softc *, int, struct isa_regs *, int);

static device_method_t ebus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ebus_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	ebus_print_child),
	DEVMETHOD(bus_probe_nomatch,	ebus_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	ebus_read_ivar),
	DEVMETHOD(bus_write_ivar,	ebus_write_ivar),
	DEVMETHOD(bus_setup_intr, 	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	ebus_alloc_resource),
	DEVMETHOD(bus_get_resource_list, ebus_get_resource_list),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),

	{ 0, 0 }
};

static driver_t ebus_driver = {
	"ebus",
	ebus_methods,
	sizeof(struct ebus_softc),
};

static devclass_t ebus_devclass;

DRIVER_MODULE(ebus, pci, ebus_driver, ebus_devclass, 0, 0);

static int
ebus_probe(device_t dev)
{
	struct ebus_softc *sc;
	struct ebus_devinfo *edi;
	char name[10];
	device_t cdev;
	phandle_t node;
	char *cname;

	/*
	 * XXX: PCI specific! There should be a common cookie IVAR value for all
	 * buses! Does not really matter much here though...
	 */
	node = ofw_pci_find_node(pci_get_bus(dev), pci_get_slot(dev),
	    pci_get_function(dev));
	if (node == 0)
		return (ENXIO);

	/* Match a real ebus */
	OF_getprop(node, "name", &name, sizeof(name));
	if (pci_get_class(dev) != PCIC_BRIDGE ||
	    pci_get_vendor(dev) != 0x108e ||
	    strcmp(name, "ebus") != 0)
		return (ENXIO);

	if (pci_get_device(dev) == 0x1000)
		device_set_desc(dev, "PCI-EBus2 bridge");
	else if (pci_get_device(dev) == 0x1100)
		device_set_desc(dev, "PCI-EBus3 bridge");
	else
		return (ENXIO);

	device_printf(dev, "revision 0x%02x\n", pci_get_revid(dev));

	sc = device_get_softc(dev);
	sc->sc_node = node;

	/*
	 * Fill in our softc with information from the prom.
	 * There are two possible cases how interrupt mapping needs to be
	 * handled:
	 * - if the ebus node has an interrupt-map properties, the interrut
	 *   numbers in child nodes can be mapped using lookups in this map,
	 *   using the registers of the child node in question to find the
	 *   map entry
	 * - if it does not have such a properties, the interrupts are mapped
	 *   in the next higher interrupt map (PCI in our case), using the
	 *   interrupt number of the child, but the registers of the ebus
	 *   node, to find the mapping.
	 */
	sc->sc_imap_type = EBUS_IT_EBUS;
	sc->sc_nimap = OF_getprop_alloc(node, "interrupt-map",
	    sizeof(*sc->sc_ebus_imap), (void **)&sc->sc_ebus_imap);
	if (sc->sc_nimap == -1) {
		sc->sc_nimap = ofw_pci_find_imap(node, &sc->sc_pci_imap,
		    &sc->sc_pci_imapmsk);
		if (sc->sc_nimap == -1)
			panic("ebus_probe: no interrupt map found");
	} else {
		if (OF_getprop(node, "interrupt-map-mask",
		    &sc->sc_ebus_imapmsk, sizeof(sc->sc_ebus_imapmsk)) == -1) {
			panic("ebus_probe: could not get ebus "
			    "interrupt-map-mask");
		}
	}

	sc->sc_nrange = OF_getprop_alloc(node, "ranges",
	    sizeof(*sc->sc_range), (void **)&sc->sc_range);
	sc->sc_nreg = OF_getprop_alloc(node, "reg",
	    sizeof(*sc->sc_reg), (void **)&sc->sc_reg);
	if (sc->sc_nrange == -1 || sc->sc_nreg == -1)
		panic("ebus_attach: could not get ranges/reg property");

	/*
	 * now attach all our children
	 */
	DPRINTF(EDB_CHILD, ("ebus node %08x, searching children...\n", node));
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		if ((OF_getprop_alloc(node, "name", 1, (void **)&cname)) == -1)
			continue;

		if ((edi = ebus_setup_dinfo(sc, node, cname)) == NULL) {
			device_printf(dev, "<%s>: incomplete\n", cname);
			free(cname, M_OFWPROP);
			continue;
		}
		if ((cdev = device_add_child(dev, NULL, -1)) == NULL)
			panic("ebus_attach: device_add_child failed");
		device_set_ivars(cdev, edi);
	}
	return (0);
}

static int
ebus_print_child(device_t dev, device_t child)
{
	struct ebus_devinfo *edi;
	int retval;

	edi = device_get_ivars(child);
	retval = bus_print_child_header(dev, child);
	retval += ebus_print_res(edi);
	retval += bus_print_child_footer(dev, child);
	return (retval);
}

static void
ebus_probe_nomatch(device_t dev, device_t child)
{
	struct ebus_devinfo *edi;

	edi = device_get_ivars(child);
	device_printf(dev, "<%s>", edi->edi_name);
	ebus_print_res(edi);
	printf(" (no driver attached)\n");
}

static int
ebus_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct ebus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(child)) == NULL)
		return (ENOENT);
	switch (which) {
	case EBUS_IVAR_COMPAT:
		*result = (uintptr_t)dinfo->edi_compat;
	case EBUS_IVAR_NAME:
		*result = (uintptr_t)dinfo->edi_name;
		break;
	case EBUS_IVAR_NODE:
		*result = dinfo->edi_node;
		break;
	default:
		return (ENOENT);
	}
	return 0;
}

static int
ebus_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct ebus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(child)) == NULL)
		return (ENOENT);
	switch (which) {
	case EBUS_IVAR_COMPAT:
	case EBUS_IVAR_NAME:
	case EBUS_IVAR_NODE:
		return (EINVAL);
	default:
		return (ENOENT);
	}
	return 0;
}

static struct resource *
ebus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct ebus_softc *sc;
	struct resource_list *rl;
	struct resource_list_entry *rle;
	struct ebus_devinfo *edi;
	int passthrough = (device_get_parent(child) != bus);
	int isdefault = (start == 0UL && end == ~0UL);
	u_long nstart, nend;
	int ptype;

	sc = (struct ebus_softc *)device_get_softc(bus);
	edi = device_get_ivars(child);
	rl = &edi->edi_rl;
	/*
	 * Map ebus ranges to PCI ranges. This may include changing the
	 * allocation type.
	 */
	ptype = type;
	nstart = start;
	nend = end;
	switch (type) {
	case SYS_RES_IOPORT:
		if (!isdefault && !passthrough) {
			ptype = ofw_isa_map_iorange(sc->sc_range, sc->sc_nrange,
			    &nstart, &nend);
		}
		if (isdefault && passthrough) {
			panic("ebus_alloc_resource: passthrough of default "
			    "allocation not supported");
		}
		break;
	case SYS_RES_IRQ:
		break;
	default:
		panic("ebus_alloc_resource: unsupported resource type %d",
		    type);
	}
			
	/*
	 * This inlines a modified resource_list_alloc(); this is needed
	 * because the resources need to be mapped into the bus space.
	 * This could be done when the resources are added to the resource
	 * lists, however doing it here is slightly more flexible.
	 */
	if (passthrough) {
		return (BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
		    ptype, rid, nstart, nend, count, flags));
	}

	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL)
		return (NULL);		/* no resource of that type/rid */

	if (rle->res != NULL)
		panic("ebus_alloc_resource: resource entry is busy");

	if (isdefault) {
		nstart = rle->start;
		count = ulmax(count, rle->count);
		nend = ulmax(rle->end, nstart + count - 1);
		if (type == SYS_RES_IOPORT) {
			ptype = ofw_isa_map_iorange(sc->sc_range, sc->sc_nrange,
			    &nstart, &nend);
		}
	}

	rle->res = BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
	    ptype, rid, nstart, nend, count, flags);

	return (rle->res);

}

static struct resource_list *
ebus_get_resource_list(device_t dev, device_t child)
{
	struct ebus_devinfo *edi;

	edi = device_get_ivars(child);
	return (&edi->edi_rl);
}

static struct ebus_devinfo *
ebus_setup_dinfo(struct ebus_softc *sc, phandle_t node, char *name)
{
	struct ebus_devinfo *edi;
	struct isa_regs *reg;
	u_int32_t *intrs;
	u_int64_t start;
	int nreg, nintr, i, intr;

	edi = malloc(sizeof(*edi), M_DEVBUF, M_ZERO | M_WAITOK);
	if (edi == NULL)
		return (NULL);
	resource_list_init(&edi->edi_rl);
	edi->edi_name = name;
	edi->edi_node = node;

	OF_getprop_alloc(node, "compat", 1, (void **)&edi->edi_compat);
	nreg = OF_getprop_alloc(node, "reg", sizeof(*reg), (void **)&reg);
	if (nreg == -1) {
		ebus_destroy_dinfo(edi);
		return (NULL);
	}
	for (i = 0; i < nreg; i++) {
		start = ISA_REG_PHYS(reg + i);
		/*
		 * XXX: use SYS_RES_IOPORT for compatability with ISA drivers -
		 * probably, SYS_RES_MEMORY would be more apporpriate, although
		 * that does not really matter.
		 */
		resource_list_add(&edi->edi_rl, SYS_RES_IOPORT, i,
		    start, start + reg[i].size, reg[i].size);
	}

	nintr = OF_getprop_alloc(node, "interrupts",  sizeof(*intrs),
	    (void **)&intrs);
	for (i = 0; i < nintr; i++) {
		intr = ebus_map_intr(sc, intrs[i], reg, nreg);
		if (intr == -1)
			panic("ebus_setup_dinfo: could not map ebus "
			    "interrupt %d", intrs[i]);
		resource_list_add(&edi->edi_rl, SYS_RES_IRQ, i,
		    intr, intr, 1);
	}

	return (edi);
}

/*
 * NOTE: This does not free the name member (it is needed afterwars in some
 * cases).
 */
static void
ebus_destroy_dinfo(struct ebus_devinfo *edi)
{

	resource_list_free(&edi->edi_rl);
	free(edi, M_DEVBUF);
}


static int
ebus_print_res(struct ebus_devinfo *edi)
{
	int retval;

	retval = 0;
	retval += resource_list_print_type(&edi->edi_rl, "addr", SYS_RES_IOPORT,
	    "%#lx");
	retval += resource_list_print_type(&edi->edi_rl, "irq", SYS_RES_IRQ,
	    "%ld");
	return (retval);
}

static int
ebus_map_intr(struct ebus_softc *sc, int intr, struct isa_regs *regs,
    int nregs)
{
	int rv;

	if (sc->sc_imap_type == EBUS_IT_PCI) {
		rv = ofw_pci_route_intr2(intr, sc->sc_reg, sc->sc_pci_imap,
		    sc->sc_nimap, &sc->sc_pci_imapmsk);
		if (rv == 255)
			return (-1);
		return (rv);
	}
	return (ofw_isa_map_intr(sc->sc_ebus_imap, sc->sc_nimap,
	    &sc->sc_ebus_imapmsk, intr, regs, nregs));
}
