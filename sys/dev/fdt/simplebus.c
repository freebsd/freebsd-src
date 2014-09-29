/*-
 * Copyright (c) 2013 Nathan Whitehorn
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/rman.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

struct simplebus_range {
	uint64_t bus;
	uint64_t host;
	uint64_t size;
};

struct simplebus_softc {
	device_t dev;
	phandle_t node;

	struct simplebus_range *ranges;
	int nranges;

	pcell_t acells, scells;
};

struct simplebus_devinfo {
	struct ofw_bus_devinfo	obdinfo;
	struct resource_list	rl;
};

/*
 * Bus interface.
 */
static int		simplebus_probe(device_t dev);
static int		simplebus_attach(device_t dev);
static struct resource *simplebus_alloc_resource(device_t, device_t, int,
    int *, u_long, u_long, u_long, u_int);
static void		simplebus_probe_nomatch(device_t bus, device_t child);
static int		simplebus_print_child(device_t bus, device_t child);

/*
 * ofw_bus interface
 */
static const struct ofw_bus_devinfo *simplebus_get_devinfo(device_t bus,
    device_t child);

/*
 * local methods
 */

static int simplebus_fill_ranges(phandle_t node,
    struct simplebus_softc *sc);
static struct simplebus_devinfo *simplebus_setup_dinfo(device_t dev,
    phandle_t node);

/*
 * Driver methods.
 */
static device_method_t	simplebus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		simplebus_probe),
	DEVMETHOD(device_attach,	simplebus_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	simplebus_print_child),
	DEVMETHOD(bus_probe_nomatch,	simplebus_probe_nomatch),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	simplebus_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_adjust_resource),
	DEVMETHOD(bus_child_pnpinfo_str, ofw_bus_gen_child_pnpinfo_str),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	simplebus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

static driver_t simplebus_driver = {
	"simplebus",
	simplebus_methods,
	sizeof(struct simplebus_softc)
};
static devclass_t simplebus_devclass;
EARLY_DRIVER_MODULE(simplebus, ofwbus, simplebus_driver, simplebus_devclass,
    0, 0, BUS_PASS_BUS);
EARLY_DRIVER_MODULE(simplebus, simplebus, simplebus_driver, simplebus_devclass,
    0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);

static int
simplebus_probe(device_t dev)
{
 
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "simple-bus") &&
	    (ofw_bus_get_type(dev) == NULL || strcmp(ofw_bus_get_type(dev),
	     "soc") != 0))
		return (ENXIO);

	device_set_desc(dev, "Flattened device tree simple bus");

	return (BUS_PROBE_GENERIC);
}

static int
simplebus_attach(device_t dev)
{
	struct		simplebus_softc *sc;
	struct		simplebus_devinfo *di;
	phandle_t	node;
	device_t	cdev;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);

	sc->dev = dev;
	sc->node = node;

	/*
	 * Some important numbers
	 */
	sc->acells = 2;
	OF_getencprop(node, "#address-cells", &sc->acells, sizeof(sc->acells));
	sc->scells = 1;
	OF_getencprop(node, "#size-cells", &sc->scells, sizeof(sc->scells));

	if (simplebus_fill_ranges(node, sc) < 0) {
		device_printf(dev, "could not get ranges\n");
		return (ENXIO);
	}

	/*
	 * In principle, simplebus could have an interrupt map, but ignore that
	 * for now
	 */

	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		if ((di = simplebus_setup_dinfo(dev, node)) == NULL)
			continue;
		cdev = device_add_child(dev, NULL, -1);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    di->obdinfo.obd_name);
			resource_list_free(&di->rl);
			ofw_bus_gen_destroy_devinfo(&di->obdinfo);
			free(di, M_DEVBUF);
			continue;
		}
		device_set_ivars(cdev, di);
	}

	return (bus_generic_attach(dev));
}

static int
simplebus_fill_ranges(phandle_t node, struct simplebus_softc *sc)
{
	int host_address_cells;
	cell_t *base_ranges;
	ssize_t nbase_ranges;
	int err;
	int i, j, k;

	err = OF_searchencprop(OF_parent(node), "#address-cells",
	    &host_address_cells, sizeof(host_address_cells));
	if (err <= 0)
		return (-1);

	nbase_ranges = OF_getproplen(node, "ranges");
	if (nbase_ranges < 0)
		return (-1);
	sc->nranges = nbase_ranges / sizeof(cell_t) /
	    (sc->acells + host_address_cells + sc->scells);
	if (sc->nranges == 0)
		return (0);

	sc->ranges = malloc(sc->nranges * sizeof(sc->ranges[0]),
	    M_DEVBUF, M_WAITOK);
	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "ranges", base_ranges, nbase_ranges);

	for (i = 0, j = 0; i < sc->nranges; i++) {
		sc->ranges[i].bus = 0;
		for (k = 0; k < sc->acells; k++) {
			sc->ranges[i].bus <<= 32;
			sc->ranges[i].bus |= base_ranges[j++];
		}
		sc->ranges[i].host = 0;
		for (k = 0; k < host_address_cells; k++) {
			sc->ranges[i].host <<= 32;
			sc->ranges[i].host |= base_ranges[j++];
		}
		sc->ranges[i].size = 0;
		for (k = 0; k < sc->scells; k++) {
			sc->ranges[i].size <<= 32;
			sc->ranges[i].size |= base_ranges[j++];
		}
	}

	free(base_ranges, M_DEVBUF);
	return (sc->nranges);
}

static struct simplebus_devinfo *
simplebus_setup_dinfo(device_t dev, phandle_t node)
{
	struct simplebus_softc *sc;
	struct simplebus_devinfo *ndi;
	uint32_t *reg;
	uint64_t phys, size;
	int i, j, k;
	int nreg;

	sc = device_get_softc(dev);

	ndi = malloc(sizeof(*ndi), M_DEVBUF, M_WAITOK | M_ZERO);
	if (ofw_bus_gen_setup_devinfo(&ndi->obdinfo, node) != 0) {
		free(ndi, M_DEVBUF);
		return (NULL);
	}

	resource_list_init(&ndi->rl);
	nreg = OF_getencprop_alloc(node, "reg", sizeof(*reg), (void **)&reg);
	if (nreg == -1)
		nreg = 0;
	if (nreg % (sc->acells + sc->scells) != 0) {
		if (bootverbose)
			device_printf(dev, "Malformed reg property on <%s>\n",
			    ndi->obdinfo.obd_name);
		nreg = 0;
	}

	for (i = 0, k = 0; i < nreg; i += sc->acells + sc->scells, k++) {
		phys = size = 0;
		for (j = 0; j < sc->acells; j++) {
			phys <<= 32;
			phys |= reg[i + j];
		}
		for (j = 0; j < sc->scells; j++) {
			size <<= 32;
			size |= reg[i + sc->acells + j];
		}
		
		resource_list_add(&ndi->rl, SYS_RES_MEMORY, k,
		    phys, phys + size - 1, size);
	}
	free(reg, M_OFWPROP);

	ofw_bus_intr_to_rl(dev, node, &ndi->rl);

	return (ndi);
}

static const struct ofw_bus_devinfo *
simplebus_get_devinfo(device_t bus __unused, device_t child)
{
        struct simplebus_devinfo *ndi;
        
        ndi = device_get_ivars(child);
        return (&ndi->obdinfo);
}

static struct resource *
simplebus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct simplebus_softc *sc;
	struct simplebus_devinfo *di;
	struct resource_list_entry *rle;
	int j;

	sc = device_get_softc(bus);

	/*
	 * Request for the default allocation with a given rid: use resource
	 * list stored in the local device info.
	 */
	if ((start == 0UL) && (end == ~0UL)) {
		if ((di = device_get_ivars(child)) == NULL)
			return (NULL);

		if (type == SYS_RES_IOPORT)
			type = SYS_RES_MEMORY;

		rle = resource_list_find(&di->rl, type, *rid);
		if (rle == NULL) {
			if (bootverbose)
				device_printf(bus, "no default resources for "
				    "rid = %d, type = %d\n", *rid, type);
			return (NULL);
		}
		start = rle->start;
		end = rle->end;
		count = rle->count;
        }

	if (type == SYS_RES_MEMORY) {
		/* Remap through ranges property */
		for (j = 0; j < sc->nranges; j++) {
			if (start >= sc->ranges[j].bus && end <
			    sc->ranges[j].bus + sc->ranges[j].size) {
				start -= sc->ranges[j].bus;
				start += sc->ranges[j].host;
				end -= sc->ranges[j].bus;
				end += sc->ranges[j].host;
				break;
			}
		}
		if (j == sc->nranges && sc->nranges != 0) {
			if (bootverbose)
				device_printf(bus, "Could not map resource "
				    "%#lx-%#lx\n", start, end);

			return (NULL);
		}
	}

	return (bus_generic_alloc_resource(bus, child, type, rid, start, end,
	    count, flags));
}

static int
simplebus_print_res(struct simplebus_devinfo *di)
{
	int rv;

	rv = 0;
	rv += resource_list_print_type(&di->rl, "mem", SYS_RES_MEMORY, "%#lx");
	rv += resource_list_print_type(&di->rl, "irq", SYS_RES_IRQ, "%ld");
	return (rv);
}

static void
simplebus_probe_nomatch(device_t bus, device_t child)
{
	const char *name, *type, *compat;

	if (!bootverbose)
		return;

	name = ofw_bus_get_name(child);
	type = ofw_bus_get_type(child);
	compat = ofw_bus_get_compat(child);

	device_printf(bus, "<%s>", name != NULL ? name : "unknown");
	simplebus_print_res(device_get_ivars(child));
	if (!ofw_bus_status_okay(child))
		printf(" disabled");
	if (type)
		printf(" type %s", type);
	if (compat)
		printf(" compat %s", compat);
	printf(" (no driver attached)\n");
}

static int
simplebus_print_child(device_t bus, device_t child)
{
	int rv;

	rv = bus_print_child_header(bus, child);
	rv += simplebus_print_res(device_get_ivars(child));
	if (!ofw_bus_status_okay(child))
		rv += printf(" disabled");
	rv += bus_print_child_footer(bus, child);
	return (rv);
}
