/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/arm/gic_common.h>
#include "gic_v3_reg.h"
#include "gic_v3_var.h"

/*
 * FDT glue.
 */
static int gic_v3_fdt_probe(device_t);
static int gic_v3_fdt_attach(device_t);

static const struct ofw_bus_devinfo *gic_v3_ofw_get_devinfo(device_t, device_t);
static bus_get_resource_list_t gic_v3_fdt_get_resource_list;

static device_method_t gic_v3_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gic_v3_fdt_probe),
	DEVMETHOD(device_attach,	gic_v3_fdt_attach),

	/* Bus interface */
	DEVMETHOD(bus_get_resource_list,	gic_v3_fdt_get_resource_list),
	DEVMETHOD(bus_get_device_path,  ofw_bus_gen_get_device_path),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	gic_v3_ofw_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(gic, gic_v3_fdt_driver, gic_v3_fdt_methods,
    sizeof(struct gic_v3_softc), gic_v3_driver);

EARLY_DRIVER_MODULE(gic_v3, simplebus, gic_v3_fdt_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(gic_v3, ofwbus, gic_v3_fdt_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

/*
 * Helper functions declarations.
 */
static int gic_v3_ofw_bus_attach(device_t);

/*
 * Device interface.
 */
static int
gic_v3_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "arm,gic-v3"))
		return (ENXIO);

	device_set_desc(dev, GIC_V3_DEVSTR);
	return (BUS_PROBE_DEFAULT);
}

static int
gic_v3_fdt_attach(device_t dev)
{
	struct gic_v3_softc *sc;
	pcell_t redist_regions;
	intptr_t xref;
	int err;
	uint32_t *mbi_ranges;
	ssize_t ret;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->gic_bus = GIC_BUS_FDT;

	/*
	 * Recover number of the Re-Distributor regions.
	 */
	if (OF_getencprop(ofw_bus_get_node(dev), "#redistributor-regions",
	    &redist_regions, sizeof(redist_regions)) <= 0)
		sc->gic_redists.nregions = 1;
	else
		sc->gic_redists.nregions = redist_regions;

	/* Add Message Based Interrupts using SPIs. */
	ret = OF_getencprop_alloc_multi(ofw_bus_get_node(dev), "mbi-ranges",
	    sizeof(*mbi_ranges), (void **)&mbi_ranges);
	if (ret > 0) {
		if (ret % 2 == 0) {
			/* Limit to a single range for now. */
			sc->gic_mbi_start = mbi_ranges[0];
			sc->gic_mbi_end = mbi_ranges[0] + mbi_ranges[1];
		} else {
			if (bootverbose)
				device_printf(dev, "Malformed mbi-ranges property\n");
		}
		free(mbi_ranges, M_OFWPROP);
	}

	err = gic_v3_attach(dev);
	if (err != 0)
		goto error;

	xref = OF_xref_from_node(ofw_bus_get_node(dev));
	sc->gic_pic = intr_pic_register(dev, xref);
	if (sc->gic_pic == NULL) {
		device_printf(dev, "could not register PIC\n");
		err = ENXIO;
		goto error;
	}

	if (sc->gic_mbi_start > 0)
		intr_msi_register(dev, xref);

	/* Register xref */
	OF_device_register_xref(xref, dev);

	err = intr_pic_claim_root(dev, xref, arm_gic_v3_intr, sc, INTR_ROOT_IRQ);
	if (err != 0) {
		err = ENXIO;
		goto error;
	}

#ifdef SMP
	err = intr_ipi_pic_register(dev, 0);
	if (err != 0) {
		device_printf(dev, "could not register for IPIs\n");
		goto error;
	}
#endif

	/*
	 * Try to register ITS to this GIC.
	 * GIC will act as a bus in that case.
	 * Failure here will not affect main GIC functionality.
	 */
	if (gic_v3_ofw_bus_attach(dev) != 0) {
		if (bootverbose) {
			device_printf(dev,
			    "Failed to attach ITS to this GIC\n");
		}
	}

	if (device_get_children(dev, &sc->gic_children, &sc->gic_nchildren) != 0)
		sc->gic_nchildren = 0;

	return (err);

error:
	if (bootverbose) {
		device_printf(dev,
		    "Failed to attach. Error %d\n", err);
	}
	/* Failure so free resources */
	gic_v3_detach(dev);

	return (err);
}

/* OFW bus interface */
struct gic_v3_ofw_devinfo {
	struct gic_v3_devinfo	di_gic_dinfo;
	struct ofw_bus_devinfo	di_dinfo;
	struct resource_list	di_rl;
};

static const struct ofw_bus_devinfo *
gic_v3_ofw_get_devinfo(device_t bus __unused, device_t child)
{
	struct gic_v3_ofw_devinfo *di;

	di = device_get_ivars(child);
	if (di->di_gic_dinfo.is_vgic)
		return (NULL);
	return (&di->di_dinfo);
}

/* Helper functions */
static int
gic_v3_ofw_fill_ranges(phandle_t parent, struct gic_v3_softc *sc,
    pcell_t *addr_cellsp, pcell_t *size_cellsp)
{
	pcell_t addr_cells, host_cells, size_cells;
	cell_t *base_ranges;
	ssize_t nbase_ranges;
	int i, j, k;

	host_cells = 1;
	OF_getencprop(OF_parent(parent), "#address-cells", &host_cells,
	    sizeof(host_cells));
	addr_cells = 2;
	OF_getencprop(parent, "#address-cells", &addr_cells,
	    sizeof(addr_cells));
	size_cells = 2;
	OF_getencprop(parent, "#size-cells", &size_cells,
	    sizeof(size_cells));

	*addr_cellsp = addr_cells;
	*size_cellsp = size_cells;

	nbase_ranges = OF_getproplen(parent, "ranges");
	if (nbase_ranges < 0)
		return (EINVAL);

	sc->nranges = nbase_ranges / sizeof(cell_t) /
	    (addr_cells + host_cells + size_cells);
	if (sc->nranges == 0)
		return (0);

	sc->ranges = malloc(sc->nranges * sizeof(sc->ranges[0]), M_GIC_V3,
	    M_WAITOK);
	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getencprop(parent, "ranges", base_ranges, nbase_ranges);

	for (i = 0, j = 0; i < sc->nranges; i++) {
		sc->ranges[i].bus = 0;
		for (k = 0; k < addr_cells; k++) {
			sc->ranges[i].bus <<= 32;
			sc->ranges[i].bus |= base_ranges[j++];
		}
		sc->ranges[i].host = 0;
		for (k = 0; k < host_cells; k++) {
			sc->ranges[i].host <<= 32;
			sc->ranges[i].host |= base_ranges[j++];
		}
		sc->ranges[i].size = 0;
		for (k = 0; k < size_cells; k++) {
			sc->ranges[i].size <<= 32;
			sc->ranges[i].size |= base_ranges[j++];
		}
	}

	free(base_ranges, M_DEVBUF);
	return (0);
}

/*
 * Bus capability support for GICv3.
 * Collects and configures device informations and finally
 * adds ITS device as a child of GICv3 in Newbus hierarchy.
 */
static int
gic_v3_ofw_bus_attach(device_t dev)
{
	struct gic_v3_ofw_devinfo *di;
	struct gic_v3_softc *sc;
	device_t child;
	phandle_t parent, node;
	pcell_t addr_cells, size_cells;
	int rv;

	sc = device_get_softc(dev);
	parent = ofw_bus_get_node(dev);
	if (parent > 0) {
		rv = gic_v3_ofw_fill_ranges(parent, sc, &addr_cells,
		    &size_cells);
		if (rv != 0)
			return (rv);

		/* Iterate through all GIC subordinates */
		for (node = OF_child(parent); node > 0; node = OF_peer(node)) {
			/*
			 * Ignore children that lack a compatible property.
			 * Some of them may be for configuration, for example
			 * ppi-partitions.
			 */
			if (!OF_hasprop(node, "compatible"))
				continue;

			/* Allocate and populate devinfo. */
			di = malloc(sizeof(*di), M_GIC_V3, M_WAITOK | M_ZERO);

			/* Read the numa node, or -1 if there is none */
			if (OF_getencprop(node, "numa-node-id",
			    &di->di_gic_dinfo.gic_domain,
			    sizeof(di->di_gic_dinfo.gic_domain)) <= 0) {
				di->di_gic_dinfo.gic_domain = -1;
			}

			if (ofw_bus_gen_setup_devinfo(&di->di_dinfo, node)) {
				if (bootverbose) {
					device_printf(dev,
					    "Could not set up devinfo for ITS\n");
				}
				free(di, M_GIC_V3);
				continue;
			}

			/* Initialize and populate resource list. */
			resource_list_init(&di->di_rl);
			ofw_bus_reg_to_rl(dev, node, addr_cells, size_cells,
			    &di->di_rl);

			/* Should not have any interrupts, so don't add any */

			/* Add newbus device for this FDT node */
			child = device_add_child(dev, NULL, DEVICE_UNIT_ANY);
			if (!child) {
				if (bootverbose) {
					device_printf(dev,
					    "Could not add child: %s\n",
					    di->di_dinfo.obd_name);
				}
				resource_list_free(&di->di_rl);
				ofw_bus_gen_destroy_devinfo(&di->di_dinfo);
				free(di, M_GIC_V3);
				continue;
			}

			sc->gic_nchildren++;
			device_set_ivars(child, di);
		}
	}

	/*
	 * If there is a vgic maintanance interrupt add a virtual gic
	 * child so we can use this in the vmm module for bhyve.
	 */
	if (OF_hasprop(parent, "interrupts")) {
		child = device_add_child(dev, "vgic", DEVICE_UNIT_ANY);
		if (child == NULL) {
			device_printf(dev, "Could not add vgic child\n");
		} else {
			di = malloc(sizeof(*di), M_GIC_V3, M_WAITOK | M_ZERO);
			resource_list_init(&di->di_rl);
			di->di_gic_dinfo.gic_domain = -1;
			di->di_gic_dinfo.is_vgic = 1;
			device_set_ivars(child, di);
			sc->gic_nchildren++;
		}
	}

	bus_attach_children(dev);
	return (0);
}

static struct resource_list *
gic_v3_fdt_get_resource_list(device_t bus, device_t child)
{
	struct gic_v3_ofw_devinfo *di;

	di = device_get_ivars(child);
	KASSERT(di != NULL, ("%s: No devinfo", __func__));

	return (&di->di_rl);
}
