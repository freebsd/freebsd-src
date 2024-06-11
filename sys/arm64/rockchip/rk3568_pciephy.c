/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, 2022 Soren Schmidt <sos@deepcore.dk>
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
 *
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/simple_mfd.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/regulator/regulator.h>
#include <dev/syscon/syscon.h>
#include <dev/phy/phy.h>

#include <contrib/device-tree/include/dt-bindings/phy/phy.h>

#include "syscon_if.h"
#include "phydev_if.h"
#include "phynode_if.h"

#define	GRF_PCIE30PHY_CON1		0x04
#define	GRF_PCIE30PHY_CON4		0x10
#define	GRF_PCIE30PHY_CON5		0x14
#define	GRF_PCIE30PHY_CON6		0x18
#define	 GRF_BIFURCATION_LANE_1		0
#define	 GRF_BIFURCATION_LANE_2		1
#define	 GRF_PCIE30PHY_WR_EN		(0xf << 16)
#define	GRF_PCIE30PHY_CON9		0x24
#define	 GRF_PCIE30PHY_DA_OCM_MASK	(1 << (15 + 16))
#define	 GRF_PCIE30PHY_DA_OCM		((1 << 15) | GRF_PCIE30PHY_DA_OCM_MASK)
#define	GRF_PCIE30PHY_STATUS0		0x80
#define	 SRAM_INIT_DONE			(1 << 14)

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3568-pcie3-phy",	1},
	{NULL, 0}
};

struct rk3568_pciephy_softc {
	device_t	dev;
	phandle_t	node;
	struct resource	*mem;
	struct phynode	*phynode;
	struct syscon	*phy_grf;
	clk_t		refclk_m;
	clk_t		refclk_n;
	clk_t		pclk;
	hwreset_t	phy_reset;
};


static void
rk3568_pciephy_bifurcate(device_t dev, int control, uint32_t lane)
{
	struct rk3568_pciephy_softc *sc = device_get_softc(dev);

	switch (lane) {
	case 0:
		SYSCON_WRITE_4(sc->phy_grf, control, GRF_PCIE30PHY_WR_EN);
		return;
	case 1:
		SYSCON_WRITE_4(sc->phy_grf, control,
		    GRF_PCIE30PHY_WR_EN | GRF_BIFURCATION_LANE_1);
		break;
	case 2:
		SYSCON_WRITE_4(sc->phy_grf, control,
		    GRF_PCIE30PHY_WR_EN | GRF_BIFURCATION_LANE_2);
		break;
	default:
		device_printf(dev, "Illegal lane %d\n", lane);
		return;
	}
	if (bootverbose)
		device_printf(dev, "lane %d @ pcie3x%d\n", lane,
		    (control == GRF_PCIE30PHY_CON5) ? 1 : 2);
}

/* PHY class and methods */
static int
rk3568_pciephy_enable(struct phynode *phynode, bool enable)
{
	device_t dev = phynode_get_device(phynode);
	struct rk3568_pciephy_softc *sc = device_get_softc(dev);
	int count;

	if (enable) {
		/* Pull PHY out of reset */
		hwreset_deassert(sc->phy_reset);

		/* Poll for SRAM loaded and ready */
		for (count = 100; count; count--) {
			if (SYSCON_READ_4(sc->phy_grf, GRF_PCIE30PHY_STATUS0) &
			    SRAM_INIT_DONE)
				break;
			DELAY(10000);
			if (count == 0) {
				device_printf(dev, "SRAM init timeout!\n");
				return (ENXIO);
			}
		}
	}
	return (0);
}

static phynode_method_t rk3568_pciephy_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable,	rk3568_pciephy_enable),

	PHYNODEMETHOD_END
};
DEFINE_CLASS_1(rk3568_pciephy_phynode, rk3568_pciephy_phynode_class,
    rk3568_pciephy_phynode_methods, 0, phynode_class);


/* Device class and methods */
static int
rk3568_pciephy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "RockChip PCIe PHY");
	return (BUS_PROBE_DEFAULT);
}

static int
rk3568_pciephy_attach(device_t dev)
{
	struct rk3568_pciephy_softc *sc = device_get_softc(dev);
	struct phynode_init_def phy_init;
	struct phynode *phynode;
	uint32_t data_lanes[2] = { 0, 0 };
	int rid = 0;

	sc->dev = dev;
	sc->node = ofw_bus_get_node(dev);

	/* Get memory resource */
	if (!(sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE))) {
		device_printf(dev, "Cannot allocate memory resources\n");
		return (ENXIO);
	}

	/* Get syncons handle */
	if (OF_hasprop(sc->node, "rockchip,phy-grf") &&
	    syscon_get_by_ofw_property(dev, sc->node, "rockchip,phy-grf",
	    &sc->phy_grf))
		return (ENXIO);

	/* Get & enable clocks */
	if (clk_get_by_ofw_name(dev, 0, "refclk_m", &sc->refclk_m)) {
		device_printf(dev, "getting refclk_m failed\n");
		return (ENXIO);
	}
	if (clk_enable(sc->refclk_m))
		device_printf(dev, "enable refclk_m failed\n");
	if (clk_get_by_ofw_name(dev, 0, "refclk_n", &sc->refclk_n)) {
		device_printf(dev, "getting refclk_n failed\n");
		return (ENXIO);
	}
	if (clk_enable(sc->refclk_n))
		device_printf(dev, "enable refclk_n failed\n");
	if (clk_get_by_ofw_name(dev, 0, "pclk", &sc->pclk)) {
		device_printf(dev, "getting pclk failed\n");
		return (ENXIO);
	}
	if (clk_enable(sc->pclk))
		device_printf(dev, "enable pclk failed\n");

	/* Get & assert reset */
	if (hwreset_get_by_ofw_idx(dev, sc->node, 0, &sc->phy_reset)) {
		device_printf(dev, "Cannot get reset\n");
	} else
		hwreset_assert(sc->phy_reset);

	/* Set RC/EP mode not implemented yet (RC mode only) */

	/* Set bifurcation according to "data-lanes" entry */
	if (OF_hasprop(sc->node, "data-lanes")) {
		OF_getencprop(sc->node, "data-lanes", data_lanes,
		    sizeof(data_lanes));
	} else
		if (bootverbose)
			device_printf(dev, "lane 1 & 2 @pcie3x2\n");

	/* Deassert PCIe PMA output clamp mode */
	SYSCON_WRITE_4(sc->phy_grf, GRF_PCIE30PHY_CON9, GRF_PCIE30PHY_DA_OCM);

	/* Configure PHY HW accordingly */
	rk3568_pciephy_bifurcate(dev, GRF_PCIE30PHY_CON5, data_lanes[0]);
	rk3568_pciephy_bifurcate(dev, GRF_PCIE30PHY_CON6, data_lanes[1]);

	if (data_lanes[0] || data_lanes[1])
		SYSCON_WRITE_4(sc->phy_grf, GRF_PCIE30PHY_CON1,
		    GRF_PCIE30PHY_DA_OCM);
	else
		SYSCON_WRITE_4(sc->phy_grf, GRF_PCIE30PHY_CON1,
		    GRF_PCIE30PHY_DA_OCM_MASK);

	bzero(&phy_init, sizeof(phy_init));
	phy_init.id = PHY_NONE;
	phy_init.ofw_node = sc->node;
	if (!(phynode = phynode_create(dev, &rk3568_pciephy_phynode_class,
	    &phy_init))) {
		device_printf(dev, "failed to create pciephy PHY\n");
		return (ENXIO);
	}
	if (!phynode_register(phynode)) {
		device_printf(dev, "failed to register pciephy PHY\n");
		return (ENXIO);
	}
	sc->phynode = phynode;

	return (0);
}

static device_method_t rk3568_pciephy_methods[] = {
	DEVMETHOD(device_probe,		rk3568_pciephy_probe),
	DEVMETHOD(device_attach,	rk3568_pciephy_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk3568_pciephy, rk3568_pciephy_driver, rk3568_pciephy_methods,
    sizeof(struct simple_mfd_softc), simple_mfd_driver);
EARLY_DRIVER_MODULE(rk3568_pciephy, simplebus, rk3568_pciephy_driver,
    0, 0, BUS_PASS_RESOURCE + BUS_PASS_ORDER_LATE);
