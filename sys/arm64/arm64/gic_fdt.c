/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm64/arm64/gic.h>

static struct ofw_compat_data compat_data[] = {
	{"arm,gic",		true},	/* Non-standard, used in FreeBSD dts. */
	{"arm,gic-400",		true},
	{"arm,cortex-a15-gic",	true},
	{"arm,cortex-a9-gic",	true},
	{"arm,cortex-a7-gic",	true},
	{"arm,arm11mp-gic",	true},
	{"brcm,brahma-b15-gic",	true},
	{"qcom,msm-qgic2",	true},
	{NULL,			false}
};

struct gic_range {
	uint64_t bus;
	uint64_t host;
	uint64_t size;
};

struct arm_gic_fdt_softc {
	struct arm_gic_softc sc_gic;
	pcell_t		sc_host_cells;
	pcell_t		sc_addr_cells;
	pcell_t		sc_size_cells;
	struct gic_range *sc_ranges;
	int		sc_nranges;
};

struct gic_devinfo {
	struct ofw_bus_devinfo	obdinfo;
	struct resource_list	rl;
};

static int
gic_fill_ranges(phandle_t node, struct arm_gic_fdt_softc *sc)
{
	cell_t *base_ranges;
	ssize_t nbase_ranges;
	int i, j, k;

	nbase_ranges = OF_getproplen(node, "ranges");
	if (nbase_ranges < 0)
		return (-1);
	sc->sc_nranges = nbase_ranges / sizeof(cell_t) /
	    (sc->sc_addr_cells + sc->sc_host_cells + sc->sc_size_cells);
	if (sc->sc_nranges == 0)
		return (0);

	sc->sc_ranges = malloc(sc->sc_nranges * sizeof(sc->sc_ranges[0]),
	    M_DEVBUF, M_WAITOK);
	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "ranges", base_ranges, nbase_ranges);

	for (i = 0, j = 0; i < sc->sc_nranges; i++) {
		sc->sc_ranges[i].bus = 0;
		for (k = 0; k < sc->sc_addr_cells; k++) {
			sc->sc_ranges[i].bus <<= 32;
			sc->sc_ranges[i].bus |= base_ranges[j++];
		}
		sc->sc_ranges[i].host = 0;
		for (k = 0; k < sc->sc_host_cells; k++) {
			sc->sc_ranges[i].host <<= 32;
			sc->sc_ranges[i].host |= base_ranges[j++];
		}
		sc->sc_ranges[i].size = 0;
		for (k = 0; k < sc->sc_size_cells; k++) {
			sc->sc_ranges[i].size <<= 32;
			sc->sc_ranges[i].size |= base_ranges[j++];
		}
	}

	free(base_ranges, M_DEVBUF);
	return (sc->sc_nranges);
}

static int
arm_gic_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "ARM Generic Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
arm_gic_fdt_attach(device_t dev)
{
	struct arm_gic_fdt_softc *sc = device_get_softc(dev);
	phandle_t root, child;
	struct gic_devinfo *dinfo;
	device_t cdev;
	int err;

	err = arm_gic_attach(dev);
	if (err != 0)
		return (err);

	root = ofw_bus_get_node(dev);

	sc->sc_host_cells = 1;
	OF_getencprop(OF_parent(root), "#address-cells", &sc->sc_host_cells,
	    sizeof(sc->sc_host_cells));
	sc->sc_addr_cells = 2;
	OF_getencprop(root, "#address-cells", &sc->sc_addr_cells,
	    sizeof(sc->sc_addr_cells));
	sc->sc_size_cells = 2;
	OF_getencprop(root, "#size-cells", &sc->sc_size_cells,
	    sizeof(sc->sc_size_cells));

	/* If we have no children don't probe for them */
	child = OF_child(root);
	if (child == 0)
		return (0);

	if (gic_fill_ranges(root, sc) < 0) {
		device_printf(dev, "could not get ranges\n");
		return (ENXIO);
	}

	for (; child != 0; child = OF_peer(child)) {
		dinfo = malloc(sizeof(*dinfo), M_DEVBUF, M_WAITOK | M_ZERO);

		if (ofw_bus_gen_setup_devinfo(&dinfo->obdinfo, child) != 0) {
			free(dinfo, M_DEVBUF);
			continue;
		}

		resource_list_init(&dinfo->rl);
		ofw_bus_reg_to_rl(dev, child, sc->sc_addr_cells,
		    sc->sc_size_cells, &dinfo->rl);

		cdev = device_add_child(dev, NULL, -1);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    dinfo->obdinfo.obd_name);
			resource_list_free(&dinfo->rl);
			ofw_bus_gen_destroy_devinfo(&dinfo->obdinfo);
			free(dinfo, M_DEVBUF);
			continue;
		}
		device_set_ivars(cdev, dinfo);
	}

	bus_generic_probe(dev);
	return (bus_generic_attach(dev));
}

static struct resource *
arm_gic_fdt_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct arm_gic_fdt_softc *sc = device_get_softc(bus);
	struct gic_devinfo *di;
	struct resource_list_entry *rle;
	int j;

	KASSERT(type == SYS_RES_MEMORY, ("Invalid resoure type %x", type));

	/*
	 * Request for the default allocation with a given rid: use resource
	 * list stored in the local device info.
	 */
	if (RMAN_IS_DEFAULT_RANGE(start, end)) {
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

	/* Remap through ranges property */
	for (j = 0; j < sc->sc_nranges; j++) {
		if (start >= sc->sc_ranges[j].bus && end <
		    sc->sc_ranges[j].bus + sc->sc_ranges[j].size) {
			start -= sc->sc_ranges[j].bus;
			start += sc->sc_ranges[j].host;
			end -= sc->sc_ranges[j].bus;
			end += sc->sc_ranges[j].host;
			break;
		}
	}
	if (j == sc->sc_nranges && sc->sc_nranges != 0) {
		if (bootverbose)
			device_printf(bus, "Could not map resource "
			    "%#lx-%#lx\n", start, end);

		return (NULL);
	}

	return (bus_generic_alloc_resource(bus, child, type, rid, start, end,
	    count, flags));
}

static const struct ofw_bus_devinfo *
arm_gic_fdt_ofw_get_devinfo(device_t bus __unused, device_t child)
{
	struct gic_devinfo *di;

	di = device_get_ivars(child);

	return (&di->obdinfo);
}


static device_method_t arm_gic_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		arm_gic_fdt_probe),
	DEVMETHOD(device_attach,	arm_gic_fdt_attach),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_alloc_resource,	arm_gic_fdt_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,bus_generic_activate_resource),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	arm_gic_fdt_ofw_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gic, arm_gic_fdt_driver, arm_gic_fdt_methods,
    sizeof(struct arm_gic_fdt_softc), arm_gic_driver);

static devclass_t arm_gic_fdt_devclass;

EARLY_DRIVER_MODULE(gic, simplebus, arm_gic_fdt_driver,
    arm_gic_fdt_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(gic, ofwbus, arm_gic_fdt_driver, arm_gic_fdt_devclass,
    0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

static struct ofw_compat_data gicv2m_compat_data[] = {
	{"arm,gic-v2m-frame",	true},
	{NULL,			false}
};

static int
arm_gicv2m_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, gicv2m_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "ARM Generic Interrupt Controller MSI/MSIX");
	return (BUS_PROBE_DEFAULT);
}

static device_method_t arm_gicv2m_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		arm_gicv2m_fdt_probe),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(gicv2m, arm_gicv2m_fdt_driver, arm_gicv2m_fdt_methods,
    sizeof(struct gicv2m_softc), arm_gicv2m_driver);

static devclass_t arm_gicv2m_fdt_devclass;

EARLY_DRIVER_MODULE(gicv2m, gic, arm_gicv2m_fdt_driver,
    arm_gicv2m_fdt_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
