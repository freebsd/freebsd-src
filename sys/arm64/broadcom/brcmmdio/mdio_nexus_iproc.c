/*-
 * Copyright (c) 2019 Juniper Networks, Inc.
 * Copyright (c) 2019 Semihalf.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include "mdio_if.h"

MALLOC_DEFINE(M_BRCM_IPROC_NEXUS, "Broadcom IPROC MDIO NEXUS",
    "Broadcom IPROC MDIO NEXUS dynamic memory");

struct brcm_mdionexus_softc {
	struct simplebus_softc simplebus_sc;
	uint32_t mux_id;
};

/* OFW bus interface */
struct brcm_mdionexus_ofw_devinfo {
	struct ofw_bus_devinfo	di_dinfo;
	struct resource_list	di_rl;
};

static device_probe_t brcm_mdionexus_fdt_probe;
static device_attach_t brcm_mdionexus_fdt_attach;

static const struct ofw_bus_devinfo * brcm_mdionexus_ofw_get_devinfo(device_t,
    device_t);
static int brcm_mdionexus_mdio_readreg(device_t, int, int);
static int brcm_mdionexus_mdio_writereg(device_t, int, int, int);

static device_method_t brcm_mdionexus_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		brcm_mdionexus_fdt_probe),
	DEVMETHOD(device_attach,	brcm_mdionexus_fdt_attach),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,		bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,		bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	brcm_mdionexus_ofw_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	/* MDIO interface */
	/* MDIO interface */
	DEVMETHOD(mdio_readreg,		brcm_mdionexus_mdio_readreg),
	DEVMETHOD(mdio_writereg,	brcm_mdionexus_mdio_writereg),

	DEVMETHOD_END
};

DEFINE_CLASS_0(brcm_mdionexus, brcm_mdionexus_fdt_driver, brcm_mdionexus_fdt_methods,
    sizeof(struct brcm_mdionexus_softc));

static driver_t brcm_mdionexus_driver = {
        "brcm_mdionexus",
	brcm_mdionexus_fdt_methods,
        sizeof(struct brcm_mdionexus_softc)
};

EARLY_DRIVER_MODULE(brcm_mdionexus, brcm_iproc_mdio, brcm_mdionexus_driver,
    NULL, NULL, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);

static int brcm_mdionexus_ofw_bus_attach(device_t);

static int
brcm_mdionexus_mdio_readreg(device_t dev, int phy, int reg)
{
	struct brcm_mdionexus_softc *sc;

	sc = device_get_softc(dev);

	return (MDIO_READREG_MUX(device_get_parent(dev),
			sc->mux_id, phy, reg));
}

static int
brcm_mdionexus_mdio_writereg(device_t dev, int phy, int reg, int val)
{
	struct brcm_mdionexus_softc *sc;

	sc = device_get_softc(dev);

	return (MDIO_WRITEREG_MUX(device_get_parent(dev),
			sc->mux_id, phy, reg, val));
}

static __inline void
get_addr_size_cells(phandle_t node, pcell_t *addr_cells, pcell_t *size_cells)
{

	*addr_cells = 2;
	/* Find address cells if present */
	OF_getencprop(node, "#address-cells", addr_cells, sizeof(*addr_cells));

	*size_cells = 2;
	/* Find size cells if present */
	OF_getencprop(node, "#size-cells", size_cells, sizeof(*size_cells));
}

static int
brcm_mdionexus_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	device_set_desc(dev, "Broadcom MDIO nexus");
	return (BUS_PROBE_SPECIFIC);
}

static int
brcm_mdionexus_fdt_attach(device_t dev)
{
	struct brcm_mdionexus_softc *sc;
	int err;
	pcell_t addr_cells, size_cells, buf[2];
	phandle_t node;

	sc = device_get_softc(dev);

	node = ofw_bus_get_node(dev);
	get_addr_size_cells(node, &addr_cells, &size_cells);
	if ((addr_cells != 1) || (size_cells != 0)) {
		device_printf(dev, "Only addr_cells=1 and size_cells=0 are supported\n");
		return (EINVAL);
	}

	if (OF_getencprop(node, "reg", buf, sizeof(pcell_t)) < 0)
		return (ENXIO);

	sc->mux_id = buf[0];

	err = brcm_mdionexus_ofw_bus_attach(dev);
	if (err != 0)
		return (err);

	bus_attach_children(dev);
	return (0);
}

static const struct ofw_bus_devinfo *
brcm_mdionexus_ofw_get_devinfo(device_t bus __unused, device_t child)
{
	struct brcm_mdionexus_ofw_devinfo *di;

	di = device_get_ivars(child);
	return (&di->di_dinfo);
}

static int
brcm_mdionexus_ofw_bus_attach(device_t dev)
{
	struct simplebus_softc *sc;
	struct brcm_mdionexus_ofw_devinfo *di;
	device_t child;
	phandle_t parent, node;

	parent = ofw_bus_get_node(dev);
	simplebus_init(dev, parent);

	sc = (struct simplebus_softc *)device_get_softc(dev);

	/* Iterate through all bus subordinates */
	for (node = OF_child(parent); node > 0; node = OF_peer(node)) {
		/* Allocate and populate devinfo. */
		di = malloc(sizeof(*di), M_BRCM_IPROC_NEXUS, M_WAITOK | M_ZERO);
		if (ofw_bus_gen_setup_devinfo(&di->di_dinfo, node) != 0) {
			free(di, M_BRCM_IPROC_NEXUS);
			continue;
		}

		/* Initialize and populate resource list. */
		resource_list_init(&di->di_rl);
		ofw_bus_reg_to_rl(dev, node, sc->acells, sc->scells,
		    &di->di_rl);
		ofw_bus_intr_to_rl(dev, node, &di->di_rl, NULL);

		/* Add newbus device for this FDT node */
		child = device_add_child(dev, NULL, DEVICE_UNIT_ANY);
		if (child == NULL) {
			resource_list_free(&di->di_rl);
			ofw_bus_gen_destroy_devinfo(&di->di_dinfo);
			free(di, M_BRCM_IPROC_NEXUS);
			continue;
		}

		device_set_ivars(child, di);
	}

	return (0);
}
