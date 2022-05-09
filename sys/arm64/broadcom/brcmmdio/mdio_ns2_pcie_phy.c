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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#define	BLK_ADDR_REG_OFFSET	0x1f
#define	PLL_AFE1_100MHZ_BLK	0x2100
#define	PLL_CLK_AMP_OFFSET	0x03
#define	PLL_CLK_AMP_2P05V	0x2b18

struct ns2_pcie_phy_softc {
	uint32_t phy_id;
};

static device_probe_t ns2_pcie_phy_fdt_probe;
static device_attach_t ns2_pcie_phy_fdt_attach;

static int ns2_pci_phy_init(device_t dev);

static device_method_t ns2_pcie_phy_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ns2_pcie_phy_fdt_probe),
	DEVMETHOD(device_attach,	ns2_pcie_phy_fdt_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ns2_pcie_phy, ns2_pcie_phy_fdt_driver, ns2_pcie_phy_fdt_methods,
    sizeof(struct ns2_pcie_phy_softc));

static driver_t ns2_pcie_phy_driver = {
        "ns2_pcie_phy",
	ns2_pcie_phy_fdt_methods,
        sizeof(struct ns2_pcie_phy_softc)
};

EARLY_DRIVER_MODULE(ns2_pcie_phy, brcm_mdionexus, ns2_pcie_phy_driver,
    NULL, NULL, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);

static int
ns2_pci_phy_init(device_t dev)
{
	struct ns2_pcie_phy_softc *sc;
	int err;

	sc = device_get_softc(dev);

	/* select the AFE 100MHz block page */
	err = MDIO_WRITEREG(device_get_parent(dev), sc->phy_id,
			    BLK_ADDR_REG_OFFSET, PLL_AFE1_100MHZ_BLK);
	if (err)
		goto err;

	/* set the 100 MHz reference clock amplitude to 2.05 v */
	err = MDIO_WRITEREG(device_get_parent(dev), sc->phy_id,
			    PLL_CLK_AMP_OFFSET, PLL_CLK_AMP_2P05V);
	if (err)
		goto err;

	return 0;

err:
	device_printf(dev, "Error %d writing to phy\n", err);
	return (err);
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
ns2_pcie_phy_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "brcm,ns2-pcie-phy"))
		return (ENXIO);

	device_set_desc(dev, "Broadcom NS2 PCIe PHY");
	return (BUS_PROBE_SPECIFIC);
}

static int
ns2_pcie_phy_fdt_attach(device_t dev)
{
	struct ns2_pcie_phy_softc *sc;
	pcell_t addr_cells, size_cells, buf[2];
	phandle_t node;

	sc = device_get_softc(dev);

	node = ofw_bus_get_node(dev);
	get_addr_size_cells(OF_parent(node), &addr_cells, &size_cells);
	if ((addr_cells != 1) || (size_cells != 0)) {
		device_printf(dev,
		    "Only addr_cells=1 and size_cells=0 are supported\n");
		return (EINVAL);
	}

	if (OF_getencprop(node, "reg", buf, sizeof(pcell_t)) < 0)
		return (ENXIO);

	sc->phy_id = buf[0];

	if (ns2_pci_phy_init(dev) < 0)
		return (EINVAL);

	return (bus_generic_attach(dev));
}
