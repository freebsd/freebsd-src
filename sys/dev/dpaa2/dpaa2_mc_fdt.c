/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Dmitry Salychev
 * Copyright © 2022 Bjoern A. Zeeb
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
/*
 * The DPAA2 Management Complex (MC) Bus Driver (FDT-based).
 *
 * MC is a hardware resource manager which can be found in several NXP
 * SoCs (LX2160A, for example) and provides an access to the specialized
 * hardware objects used in network-oriented packet processing applications.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/fdt/simplebus.h>

#include "pcib_if.h"
#include "pci_if.h"
#include "ofw_bus_if.h"

#include "dpaa2_mcp.h"
#include "dpaa2_mc.h"
#include "dpaa2_mc_if.h"

struct dpaa2_mac_fdt_softc {
	uint32_t			reg;
	phandle_t			sfp;
	phandle_t			pcs_handle;
	phandle_t			phy_handle;
	char				managed[64];
	char				phy_conn_type[64];
};

#if 0
	ethernet@1 {

		compatible = "fsl,qoriq-mc-dpmac";
		reg = <0x1>;
		sfp = <0x14>;
		pcs-handle = <0x15>;
		phy-connection-type = "10gbase-r";
		managed = "in-band-status";
	};
	ethernet@3 {

		compatible = "fsl,qoriq-mc-dpmac";
		reg = <0x3>;
		phy-handle = <0x18>;
		phy-connection-type = "qsgmii";
		managed = "in-band-status";
		pcs-handle = <0x19>;
	};
#endif

static int
dpaa2_mac_dev_probe(device_t dev)
{
	phandle_t node;
	uint64_t reg;
	ssize_t s;

	node = ofw_bus_get_node(dev);
	if (!ofw_bus_node_is_compatible(node, "fsl,qoriq-mc-dpmac")) {
		device_printf(dev, "'%s' not fsl,qoriq-mc-dpmac compatible\n",
		    ofw_bus_get_name(dev));
		return (ENXIO);
	}

	s = device_get_property(dev, "reg", &reg, sizeof(reg),
	    DEVICE_PROP_UINT32);
	if (s == -1) {
		device_printf(dev, "%s: '%s' has no 'reg' property, s %zd\n",
		    __func__, ofw_bus_get_name(dev), s);
		return (ENXIO);
	}

	device_set_desc(dev, "DPAA2 MAC DEV");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_mac_fdt_attach(device_t dev)
{
	struct dpaa2_mac_fdt_softc *sc;
	phandle_t node;
	ssize_t s;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	s = device_get_property(dev, "reg", &sc->reg, sizeof(sc->reg),
	    DEVICE_PROP_UINT32);
	if (s == -1) {
		device_printf(dev, "Cannot find 'reg' property: %zd\n", s);
		return (ENXIO);
	}

	s = device_get_property(dev, "managed", sc->managed,
	    sizeof(sc->managed), DEVICE_PROP_ANY);
	s = device_get_property(dev, "phy-connection-type", sc->phy_conn_type,
	    sizeof(sc->phy_conn_type), DEVICE_PROP_ANY);
	s = device_get_property(dev, "pcs-handle", &sc->pcs_handle,
	    sizeof(sc->pcs_handle), DEVICE_PROP_HANDLE);

	/* 'sfp' and 'phy-handle' are optional but we need one or the other. */
	s = device_get_property(dev, "sfp", &sc->sfp, sizeof(sc->sfp),
	    DEVICE_PROP_HANDLE);
	s = device_get_property(dev, "phy-handle", &sc->phy_handle,
	    sizeof(sc->phy_handle), DEVICE_PROP_HANDLE);

	if (bootverbose)
		device_printf(dev, "node %#x '%s': reg %#x sfp %#x pcs-handle "
		    "%#x phy-handle %#x managed '%s' phy-conn-type '%s'\n",
		    node, ofw_bus_get_name(dev),
		    sc->reg, sc->sfp, sc->pcs_handle, sc->phy_handle,
		    sc->managed, sc->phy_conn_type);

	return (0);
}

static bool
dpaa2_mac_fdt_match_id(device_t dev, uint32_t id)
{
	struct dpaa2_mac_fdt_softc *sc;

	if (dev == NULL)
		return (false);

	sc = device_get_softc(dev);
	if (sc->reg == id)
		return (true);

	return (false);
}

static device_t
dpaa2_mac_fdt_get_phy_dev(device_t dev)
{
	struct dpaa2_mac_fdt_softc *sc;

	if (dev == NULL)
		return (NULL);

	sc = device_get_softc(dev);
	if (sc->phy_handle == 0 && sc->sfp == 0)
		return (NULL);

#ifdef __not_yet__	/* No sff,sfp support yet. */
	if (sc->sfp != 0) {
		device_t xdev;

		xdev = OF_device_from_xref(OF_xref_from_node(sc->sfp));
		if (xdev != NULL)
			return (xdev);
	}
#endif
	return (OF_device_from_xref(OF_xref_from_node(sc->phy_handle)));
}

static device_method_t dpaa2_mac_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_mac_dev_probe),
	DEVMETHOD(device_attach,	dpaa2_mac_fdt_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(dpaa2_mac_fdt, dpaa2_mac_fdt_driver, dpaa2_mac_fdt_methods,
    sizeof(struct dpaa2_mac_fdt_softc));
DRIVER_MODULE(dpaa2_mac_fdt, dpaa2_mc, dpaa2_mac_fdt_driver, 0, 0);
MODULE_DEPEND(dpaa2_mac_fdt, memac_mdio_fdt, 1, 1, 1);

/*
 * Device interface.
 */

static int
dpaa2_mc_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "fsl,qoriq-mc"))
		return (ENXIO);

	device_set_desc(dev, "DPAA2 Management Complex");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_mc_fdt_probe_child(device_t bus, phandle_t child)
{
	device_t childdev;

	/* make sure we do not aliready have a device. */
	childdev = ofw_bus_find_child_device_by_phandle(bus, child);
	if (childdev != NULL)
		return (0);

	childdev = simplebus_add_device(bus, child, 0, "dpaa2_mac_fdt", -1,
	    NULL);
	if (childdev == NULL)
		return (ENXIO);

	return (device_probe_and_attach(childdev));
}

static int
dpaa2_mc_fdt_attach(device_t dev)
{
	struct dpaa2_mc_softc *sc;
	phandle_t node;
	phandle_t child;

	sc = device_get_softc(dev);
	sc->acpi_based = false;
	sc->ofw_node = ofw_bus_get_node(dev);

	bus_identify_children(dev);
	bus_enumerate_hinted_children(dev);

	bus_identify_children(dev);
	bus_enumerate_hinted_children(dev);

	/*
	 * Attach the children represented in the device tree.
	 */
	/* fsl-mc -> dpamcs */
	node = OF_child(sc->ofw_node);
	simplebus_init(dev, node);

	/* Attach the dpmac children represented in the device tree. */
	child = ofw_bus_find_compatible(node, "fsl,qoriq-mc-dpmac");
	for (; child > 0; child = OF_peer(child)) {
		if (!ofw_bus_node_is_compatible(child, "fsl,qoriq-mc-dpmac"))
			continue;
		if (!OF_hasprop(child, "reg"))
			continue;
		if (dpaa2_mc_fdt_probe_child(dev, child) != 0)
			continue;
	}

	return (dpaa2_mc_attach(dev));
}

/*
 * FDT compat layer.
 */
static device_t
dpaa2_mc_fdt_find_dpaa2_mac_dev(device_t dev, uint32_t id)
{
	int devcount, error, i, len;
	device_t *devlist, mdev;
	const char *mdevname;

	error = device_get_children(dev, &devlist, &devcount);
	if (error != 0)
		return (NULL);

	for (i = 0; i < devcount; i++) {
		mdev = devlist[i];
		mdevname = device_get_name(mdev);
		if (mdevname == NULL)
			continue;
		len = strlen(mdevname);
		if (strncmp("dpaa2_mac_fdt", mdevname, len) != 0)
			continue;
		if (!device_is_attached(mdev))
			continue;

		if (dpaa2_mac_fdt_match_id(mdev, id))
			return (mdev);
	}

	return (NULL);
}

static int
dpaa2_mc_fdt_get_phy_dev(device_t dev, device_t *phy_dev, uint32_t id)
{
	device_t mdev, pdev;

	mdev = dpaa2_mc_fdt_find_dpaa2_mac_dev(dev, id);
	if (mdev == NULL) {
		device_printf(dev, "%s: error finding dpmac device with id=%u\n",
		    __func__, id);
		return (ENXIO);
	}

	pdev = dpaa2_mac_fdt_get_phy_dev(mdev);
	if (pdev == NULL) {
		device_printf(dev, "%s: error getting MDIO device for dpamc %s "
		    "(id=%u)\n", __func__, device_get_nameunit(mdev), id);
		return (ENXIO);
	}

	if (phy_dev != NULL)
		*phy_dev = pdev;

	if (bootverbose)
		device_printf(dev, "dpmac_id %u mdev %p (%s) pdev %p (%s)\n",
		    id, mdev, device_get_nameunit(mdev),
		    pdev, device_get_nameunit(pdev));

	return (0);
}

static const struct ofw_bus_devinfo *
dpaa2_mc_simplebus_get_devinfo(device_t bus, device_t child)
{

	return (OFW_BUS_GET_DEVINFO(device_get_parent(bus), child));
}

static device_method_t dpaa2_mc_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_mc_fdt_probe),
	DEVMETHOD(device_attach,	dpaa2_mc_fdt_attach),
	DEVMETHOD(device_detach,	dpaa2_mc_detach),

	/* Bus interface */
	DEVMETHOD(bus_get_rman,		dpaa2_mc_rman),
	DEVMETHOD(bus_alloc_resource,	dpaa2_mc_alloc_resource),
	DEVMETHOD(bus_adjust_resource,	dpaa2_mc_adjust_resource),
	DEVMETHOD(bus_release_resource,	dpaa2_mc_release_resource),
	DEVMETHOD(bus_activate_resource, dpaa2_mc_activate_resource),
	DEVMETHOD(bus_deactivate_resource, dpaa2_mc_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* Pseudo-PCIB interface */
	DEVMETHOD(pcib_alloc_msi,	dpaa2_mc_alloc_msi),
	DEVMETHOD(pcib_release_msi,	dpaa2_mc_release_msi),
	DEVMETHOD(pcib_map_msi,		dpaa2_mc_map_msi),
	DEVMETHOD(pcib_get_id,		dpaa2_mc_get_id),

	/* DPAA2 MC bus interface */
	DEVMETHOD(dpaa2_mc_manage_dev,	dpaa2_mc_manage_dev),
	DEVMETHOD(dpaa2_mc_get_free_dev,dpaa2_mc_get_free_dev),
	DEVMETHOD(dpaa2_mc_get_dev,	dpaa2_mc_get_dev),
	DEVMETHOD(dpaa2_mc_get_shared_dev, dpaa2_mc_get_shared_dev),
	DEVMETHOD(dpaa2_mc_reserve_dev,	dpaa2_mc_reserve_dev),
	DEVMETHOD(dpaa2_mc_release_dev, dpaa2_mc_release_dev),
	DEVMETHOD(dpaa2_mc_get_phy_dev,	dpaa2_mc_fdt_get_phy_dev),

	/* OFW/simplebus */
	DEVMETHOD(ofw_bus_get_devinfo,	dpaa2_mc_simplebus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

DEFINE_CLASS_1(dpaa2_mc, dpaa2_mc_fdt_driver, dpaa2_mc_fdt_methods,
    sizeof(struct dpaa2_mc_softc), dpaa2_mc_driver);

DRIVER_MODULE(dpaa2_mc, simplebus, dpaa2_mc_fdt_driver, 0, 0);
