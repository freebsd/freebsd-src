/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * UltraSPARC 5 and beyond EBus support
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sparc64/pci/ofw_pci.h>

/*
 * The register, ranges and interrupt map properties are identical to the ISA
 * ones.
 */
#include <sparc64/isa/ofw_isa.h>

struct ebus_devinfo {
	struct ofw_bus_devinfo	edi_obdinfo;
	struct resource_list	edi_rl;
};

struct ebus_rinfo {
	int			eri_rtype;
	struct rman		eri_rman;
	struct resource		*eri_res;
};

struct ebus_softc {
	struct isa_ranges	*sc_range;
	struct ebus_rinfo	*sc_rinfo;

	int			sc_nrange;

	struct ofw_bus_iinfo	sc_iinfo;
};

static device_probe_t ebus_probe;
static device_attach_t ebus_attach;
static bus_print_child_t ebus_print_child;
static bus_probe_nomatch_t ebus_probe_nomatch;
static bus_alloc_resource_t ebus_alloc_resource;
static bus_release_resource_t ebus_release_resource;
static bus_get_resource_list_t ebus_get_resource_list;
static ofw_bus_get_devinfo_t ebus_get_devinfo;

static struct ebus_devinfo *ebus_setup_dinfo(device_t, struct ebus_softc *,
    phandle_t);
static void ebus_destroy_dinfo(struct ebus_devinfo *);
static int ebus_print_res(struct ebus_devinfo *);

static device_method_t ebus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ebus_probe),
	DEVMETHOD(device_attach,	ebus_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	ebus_print_child),
	DEVMETHOD(bus_probe_nomatch,	ebus_probe_nomatch),
	DEVMETHOD(bus_alloc_resource,	ebus_alloc_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_release_resource,	ebus_release_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_get_resource_list, ebus_get_resource_list),
	DEVMETHOD(bus_child_pnpinfo_str, ofw_bus_gen_child_pnpinfo_str),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	ebus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	KOBJMETHOD_END
};

static driver_t ebus_driver = {
	"ebus",
	ebus_methods,
	sizeof(struct ebus_softc),
};

static devclass_t ebus_devclass;

EARLY_DRIVER_MODULE(ebus, pci, ebus_driver, ebus_devclass, 0, 0,
    BUS_PASS_BUS);
MODULE_DEPEND(ebus, pci, 1, 1, 1);
MODULE_VERSION(ebus, 1);

static int
ebus_probe(device_t dev)
{

	if (pci_get_class(dev) != PCIC_BRIDGE ||
	    pci_get_vendor(dev) != 0x108e ||
	    strcmp(ofw_bus_get_name(dev), "ebus") != 0)
		return (ENXIO);

	if (pci_get_device(dev) == 0x1000)
		device_set_desc(dev, "PCI-EBus2 bridge");
	else if (pci_get_device(dev) == 0x1100)
		device_set_desc(dev, "PCI-EBus3 bridge");
	else
		return (ENXIO);
	return (0);
}

static int
ebus_attach(device_t dev)
{
	struct ebus_softc *sc;
	struct ebus_devinfo *edi;
	struct ebus_rinfo *eri;
	struct resource *res;
	device_t cdev;
	phandle_t node;
	int i, rnum, rid;

	sc = device_get_softc(dev);

	node = ofw_bus_get_node(dev);
	sc->sc_nrange = OF_getprop_alloc(node, "ranges",
	    sizeof(*sc->sc_range), (void **)&sc->sc_range);
	if (sc->sc_nrange == -1) {
		printf("ebus_attach: could not get ranges property\n");
		return (ENXIO);
	}

	sc->sc_rinfo = malloc(sizeof(*sc->sc_rinfo) * sc->sc_nrange, M_DEVBUF,
	    M_WAITOK | M_ZERO);

	/* For every range, there must be a matching resource. */
	for (rnum = 0; rnum < sc->sc_nrange; rnum++) {
		eri = &sc->sc_rinfo[rnum];
		eri->eri_rtype = ofw_isa_range_restype(&sc->sc_range[rnum]);
		rid = PCIR_BAR(rnum);
		res = bus_alloc_resource_any(dev, eri->eri_rtype, &rid,
		    RF_ACTIVE);
		if (res == NULL) {
			printf("ebus_attach: failed to allocate range "
			    "resource!\n");
			goto fail;
		}
		eri->eri_res = res;
		eri->eri_rman.rm_type = RMAN_ARRAY;
		eri->eri_rman.rm_descr = "EBus range";
		if (rman_init(&eri->eri_rman) != 0) {
			printf("ebus_attach: failed to initialize rman!");
			goto fail;
		}
		if (rman_manage_region(&eri->eri_rman, rman_get_start(res),
		    rman_get_end(res)) != 0) {
			printf("ebus_attach: failed to register region!");
			rman_fini(&eri->eri_rman);
			goto fail;
		}
	}

	ofw_bus_setup_iinfo(node, &sc->sc_iinfo, sizeof(ofw_isa_intr_t));

	/*
	 * Now attach our children.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		if ((edi = ebus_setup_dinfo(dev, sc, node)) == NULL)
			continue;
		if ((cdev = device_add_child(dev, NULL, -1)) == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    edi->edi_obdinfo.obd_name);
			ebus_destroy_dinfo(edi);
			continue;
		}
		device_set_ivars(cdev, edi);
	}
	return (bus_generic_attach(dev));

fail:
	for (i = rnum; i >= 0; i--) {
		eri = &sc->sc_rinfo[i];
		if (i < rnum)
			rman_fini(&eri->eri_rman);
		if (eri->eri_res != 0) {
			bus_release_resource(dev, eri->eri_rtype,
			    PCIR_BAR(rnum), eri->eri_res);
		}
	}
	free(sc->sc_rinfo, M_DEVBUF);
	free(sc->sc_range, M_OFWPROP);
	return (ENXIO);
}

static int
ebus_print_child(device_t dev, device_t child)
{
	int retval;

	retval = bus_print_child_header(dev, child);
	retval += ebus_print_res(device_get_ivars(child));
	retval += bus_print_child_footer(dev, child);
	return (retval);
}

static void
ebus_probe_nomatch(device_t dev, device_t child)
{

	device_printf(dev, "<%s>", ofw_bus_get_name(child));
	ebus_print_res(device_get_ivars(child));
	printf(" (no driver attached)\n");
}

static struct resource *
ebus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct ebus_softc *sc;
	struct resource_list *rl;
	struct resource_list_entry *rle = NULL;
	struct resource *res;
	struct ebus_rinfo *ri;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int passthrough = (device_get_parent(child) != bus);
	int isdefault = (start == 0UL && end == ~0UL);
	int ridx, rv;

	sc = (struct ebus_softc *)device_get_softc(bus);
	rl = BUS_GET_RESOURCE_LIST(bus, child);
	/*
	 * Map EBus ranges to PCI ranges.  This may include changing the
	 * allocation type.
	 */
	switch (type) {
	case SYS_RES_MEMORY:
		KASSERT(!(isdefault && passthrough),
		    ("ebus_alloc_resource: passthrough of default alloc"));
		if (!passthrough) {
			rle = resource_list_find(rl, type, *rid);
			if (rle == NULL)
				return (NULL);
			KASSERT(rle->res == NULL,
			    ("ebus_alloc_resource: resource entry is busy"));
			if (isdefault) {
				start = rle->start;
				count = ulmax(count, rle->count);
				end = ulmax(rle->end, start + count - 1);
			}
		}

		(void)ofw_isa_range_map(sc->sc_range, sc->sc_nrange,
		    &start, &end, &ridx);

		ri = &sc->sc_rinfo[ridx];
		res = rman_reserve_resource(&ri->eri_rman, start, end, count,
		    flags, child);
		if (res == NULL)
			return (NULL);
		rman_set_rid(res, *rid);
		bt = rman_get_bustag(ri->eri_res);
		rman_set_bustag(res, bt);
		rv = bus_space_subregion(bt, rman_get_bushandle(ri->eri_res),
		    rman_get_start(res) - rman_get_start(ri->eri_res), count,
		    &bh);
		if (rv != 0) {
			rman_release_resource(res);
			return (NULL);
		}
		rman_set_bushandle(res, bh);
		if (!passthrough)
			rle->res = res;
		return (res);
	case SYS_RES_IRQ:
		return (resource_list_alloc(rl, bus, child, type, rid, start,
		    end, count, flags));
	}
	return (NULL);
}

static int
ebus_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	struct resource_list *rl;
	struct resource_list_entry *rle;
	int passthrough = (device_get_parent(child) != bus);
	int rv;

	rl = BUS_GET_RESOURCE_LIST(bus, child);
	switch (type) {
	case SYS_RES_MEMORY:
		if ((rv = rman_release_resource(res)) != 0)
			return (rv);
		if (!passthrough) {
			rle = resource_list_find(rl, type, rid);
			KASSERT(rle != NULL, ("ebus_release_resource: "
			    "resource entry not found!"));
			KASSERT(rle->res != NULL, ("ebus_alloc_resource: "
			    "resource entry is not busy"));
			rle->res = NULL;
		}
		break;
	case SYS_RES_IRQ:
		return (resource_list_release(rl, bus, child, type, rid, res));
	default:
		panic("ebus_release_resource: unsupported resource type %d",
		    type);
	}
	return (0);
}

static struct resource_list *
ebus_get_resource_list(device_t dev, device_t child)
{
	struct ebus_devinfo *edi;

	edi = device_get_ivars(child);
	return (&edi->edi_rl);
}

static const struct ofw_bus_devinfo *
ebus_get_devinfo(device_t bus, device_t dev)
{
	struct ebus_devinfo *edi;

	edi = device_get_ivars(dev);
	return (&edi->edi_obdinfo);
}

static struct ebus_devinfo *
ebus_setup_dinfo(device_t dev, struct ebus_softc *sc, phandle_t node)
{
	struct ebus_devinfo *edi;
	struct isa_regs *reg;
	ofw_isa_intr_t *intrs;
	ofw_pci_intr_t rintr;
	u_int64_t start;
	int nreg, nintr, i;

	edi = malloc(sizeof(*edi), M_DEVBUF, M_ZERO | M_WAITOK);
	if (ofw_bus_gen_setup_devinfo(&edi->edi_obdinfo, node) != 0) {
		free(edi, M_DEVBUF);
		return (NULL);
	}
	resource_list_init(&edi->edi_rl);
	nreg = OF_getprop_alloc(node, "reg", sizeof(*reg), (void **)&reg);
	if (nreg == -1) {
		device_printf(dev, "<%s>: incomplete\n",
		    edi->edi_obdinfo.obd_name);
		goto fail;
	}
	for (i = 0; i < nreg; i++) {
		start = ISA_REG_PHYS(reg + i);
		resource_list_add(&edi->edi_rl, SYS_RES_MEMORY, i,
		    start, start + reg[i].size - 1, reg[i].size);
	}
	free(reg, M_OFWPROP);

	nintr = OF_getprop_alloc(node, "interrupts",  sizeof(*intrs),
	    (void **)&intrs);
	for (i = 0; i < nintr; i++) {
		rintr = ofw_isa_route_intr(dev, node, &sc->sc_iinfo, intrs[i]);
		if (rintr == PCI_INVALID_IRQ) {
			device_printf(dev,
			    "<%s>: could not map EBus interrupt %d\n",
			    edi->edi_obdinfo.obd_name, intrs[i]);
			free(intrs, M_OFWPROP);
			goto fail;
		}
		resource_list_add(&edi->edi_rl, SYS_RES_IRQ, i,
		    rintr, rintr, 1);
	}
	free(intrs, M_OFWPROP);

	return (edi);

fail:
	ebus_destroy_dinfo(edi);
	return (NULL);
}

static void
ebus_destroy_dinfo(struct ebus_devinfo *edi)
{

	resource_list_free(&edi->edi_rl);
	ofw_bus_gen_destroy_devinfo(&edi->edi_obdinfo);
	free(edi, M_DEVBUF);
}

static int
ebus_print_res(struct ebus_devinfo *edi)
{
	int retval;

	retval = 0;
	retval += resource_list_print_type(&edi->edi_rl, "addr", SYS_RES_MEMORY,
	    "%#lx");
	retval += resource_list_print_type(&edi->edi_rl, "irq", SYS_RES_IRQ,
	    "%ld");
	return (retval);
}
