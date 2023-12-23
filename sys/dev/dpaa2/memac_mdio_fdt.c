/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Bjoern A. Zeeb
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/fdt/simplebus.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "memac_mdio.h"
#include "memac_mdio_if.h"
#include "ofw_bus_if.h"
#include "miibus_if.h"

/* -------------------------------------------------------------------------- */

struct memacphy_softc_fdt {
	struct memacphy_softc_common	scc;
	uint32_t			reg;
	phandle_t			xref;
};

static void
memacphy_fdt_miibus_statchg(device_t dev)
{
	struct memacphy_softc_fdt *sc;

	sc = device_get_softc(dev);
	memacphy_miibus_statchg(&sc->scc);
}

static int
memacphy_fdt_set_ni_dev(device_t dev, device_t nidev)
{
	struct memacphy_softc_fdt *sc;

	sc = device_get_softc(dev);
	return (memacphy_set_ni_dev(&sc->scc, nidev));
}

static int
memacphy_fdt_get_phy_loc(device_t dev, int *phy_loc)
{
	struct memacphy_softc_fdt *sc;

	sc = device_get_softc(dev);
	return (memacphy_get_phy_loc(&sc->scc, phy_loc));
}

static int
memacphy_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "MEMAC PHY (fdt)");
	return (BUS_PROBE_DEFAULT);
}

static int
memacphy_fdt_attach(device_t dev)
{
	struct memacphy_softc_fdt *sc;
	phandle_t node;
	ssize_t s;
	int error;

	sc = device_get_softc(dev);
	sc->scc.dev = dev;
	node = ofw_bus_get_node(dev);

	s = device_get_property(dev, "reg", &sc->reg, sizeof(sc->reg),
	    DEVICE_PROP_UINT32);
	if (s != -1)
		sc->scc.phy = sc->reg;
	else
		sc->scc.phy = -1;
	sc->xref = OF_xref_from_node(node);

	error = OF_device_register_xref(sc->xref, dev);
	if (error != 0)
		device_printf(dev, "Failed to register xref %#x\n", sc->xref);

	if (bootverbose)
		device_printf(dev, "node %#x '%s': reg %#x xref %#x phy %u\n",
		    node, ofw_bus_get_name(dev), sc->reg, sc->xref, sc->scc.phy);

	if (sc->scc.phy == -1)
		error = ENXIO;
	return (error);
}

static device_method_t memacphy_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		memacphy_fdt_probe),
	DEVMETHOD(device_attach,	memacphy_fdt_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	memacphy_miibus_readreg),
	DEVMETHOD(miibus_writereg,	memacphy_miibus_writereg),
	DEVMETHOD(miibus_statchg,	memacphy_fdt_miibus_statchg),

	/* memac */
	DEVMETHOD(memac_mdio_set_ni_dev, memacphy_fdt_set_ni_dev),
	DEVMETHOD(memac_mdio_get_phy_loc, memacphy_fdt_get_phy_loc),

	DEVMETHOD_END
};

DEFINE_CLASS_0(memacphy_fdt, memacphy_fdt_driver, memacphy_fdt_methods,
    sizeof(struct memacphy_softc_fdt));

EARLY_DRIVER_MODULE(memacphy_fdt, memac_mdio_fdt, memacphy_fdt_driver, 0, 0,
    BUS_PASS_SUPPORTDEV);
DRIVER_MODULE(miibus, memacphy_fdt, miibus_driver, 0, 0);
MODULE_DEPEND(memacphy_fdt, miibus, 1, 1, 1);

/* -------------------------------------------------------------------------- */

/*
 * Order in this softc is important; memac_mdio_fdt_attach() calls
 * simplebus_init() which expects sb_sc at the beginning.
 */
struct memac_mdio_softc_fdt {
	struct simplebus_softc		sb_sc;		/* Must stay first. */
	struct memac_mdio_softc_common	scc;
};

static int
memac_fdt_miibus_readreg(device_t dev, int phy, int reg)
{
	struct memac_mdio_softc_fdt *sc;

	sc = device_get_softc(dev);
	return (memac_miibus_readreg(&sc->scc, phy, reg));
}

static int
memac_fdt_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct memac_mdio_softc_fdt *sc;

	sc = device_get_softc(dev);
	return (memac_miibus_writereg(&sc->scc, phy, reg, data));
}

static struct ofw_compat_data compat_data[] = {
	{ "fsl,fman-memac-mdio",		1 },
	{ NULL,					0 }
};

static int
memac_mdio_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Freescale XGMAC MDIO Bus (FDT)");
	return (BUS_PROBE_DEFAULT);
}

static int
memac_mdio_fdt_probe_child(device_t bus, phandle_t child)
{
	device_t childdev;

	/* Make sure we do not aliready have a device. */
	childdev = ofw_bus_find_child_device_by_phandle(bus, child);
	if (childdev != NULL)
		return (0);

	childdev = simplebus_add_device(bus, child, 0, NULL, -1, NULL);
	if (childdev == NULL)
		return (ENXIO);

	return (device_probe_and_attach(childdev));
}

static int
memac_mdio_fdt_attach(device_t dev)
{
	struct memac_mdio_softc_fdt *sc;
	phandle_t node, child;
	int error;

	sc = device_get_softc(dev);
	sc->scc.dev = dev;

	error = memac_mdio_generic_attach(&sc->scc);
	if (error != 0)
		return (error);

	/* Attach the *phy* children represented in the device tree. */
	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);
	node = ofw_bus_get_node(dev);
	simplebus_init(dev, node);
	for (child = OF_child(node); child > 0; child = OF_peer(child)) {
		if (!OF_hasprop(child, "reg"))
			continue;
		if (memac_mdio_fdt_probe_child(dev, child) != 0)
			continue;
	}

	return (0);
}

static int
memac_mdio_fdt_detach(device_t dev)
{
	struct memac_mdio_softc_fdt *sc;

	sc = device_get_softc(dev);
	return (memac_mdio_generic_detach(&sc->scc));
}

static const struct ofw_bus_devinfo *
memac_simplebus_get_devinfo(device_t bus, device_t child)
{

	return (OFW_BUS_GET_DEVINFO(device_get_parent(bus), child));
}

static device_method_t memac_mdio_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		memac_mdio_fdt_probe),
	DEVMETHOD(device_attach,	memac_mdio_fdt_attach),
	DEVMETHOD(device_detach,	memac_mdio_fdt_detach),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	memac_fdt_miibus_readreg),
	DEVMETHOD(miibus_writereg,	memac_fdt_miibus_writereg),

	/* OFW/simplebus */
	DEVMETHOD(ofw_bus_get_devinfo,	memac_simplebus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_read_ivar,	memac_mdio_read_ivar),
	DEVMETHOD(bus_get_property,	memac_mdio_get_property),

	DEVMETHOD_END
};

DEFINE_CLASS_0(memac_mdio_fdt, memac_mdio_fdt_driver, memac_mdio_fdt_methods,
    sizeof(struct memac_mdio_softc_fdt));

EARLY_DRIVER_MODULE(memac_mdio_fdt, simplebus, memac_mdio_fdt_driver, 0, 0,
    BUS_PASS_SUPPORTDEV);

DRIVER_MODULE(miibus, memac_mdio_fdt, miibus_driver, 0, 0);
MODULE_DEPEND(memac_mdio_fdt, miibus, 1, 1, 1);
MODULE_VERSION(memac_mdio_fdt, 1);
