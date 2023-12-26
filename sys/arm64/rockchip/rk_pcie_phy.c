/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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

/*
 * Rockchip PHY TYPEC
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/gpio.h>
#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/clk/clk.h>
#include <dev/phy/phy.h>
#include <dev/phy/phy_internal.h>
#include <dev/syscon/syscon.h>
#include <dev/hwreset/hwreset.h>

#include "syscon_if.h"

#define GRF_HIWORD_SHIFT	16
#define	GRF_SOC_CON_5_PCIE	0xE214
#define	 CON_5_PCIE_IDLE_OFF(x)	(1 <<(((x) & 0x3) + 3))
#define	GRF_SOC_CON8		0xE220
#define	GRF_SOC_STATUS1 	0xE2A4

/* PHY config registers  - write */
#define	PHY_CFG_CLK_TEST	0x10
#define	 CLK_TEST_SEPE_RATE		(1 << 3)
#define	PHY_CFG_CLK_SCC		0x12
#define	 CLK_SCC_PLL_100M		(1 << 3)

/* PHY config registers  - read */
#define	PHY_CFG_PLL_LOCK	0x10
#define	 CLK_PLL_LOCKED			(1 << 1)
#define	PHY_CFG_SCC_LOCK	0x12
#define	 CLK_SCC_100M_GATE		(1 << 2)

#define	 STATUS1_PLL_LOCKED		(1 << 9)

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3399-pcie-phy",	1},
	{NULL,				0}
};

struct rk_pcie_phy_softc {
	device_t		dev;
	struct syscon		*syscon;
	struct mtx		mtx;
	clk_t			clk_ref;
	hwreset_t		hwreset_phy;
	int			enable_count;
};

#define	PHY_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define	PHY_UNLOCK(_sc)		mtx_unlock(&(_sc)->mtx)
#define	PHY_LOCK_INIT(_sc)	mtx_init(&(_sc)->mtx, 			\
	    device_get_nameunit(_sc->dev), "rk_pcie_phyc", MTX_DEF)
#define	PHY_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->mtx);
#define	PHY_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED);
#define	PHY_ASSERT_UNLOCKED(_sc) mtx_assert(&(_sc)->mtx, MA_NOTOWNED);

#define	RD4(sc, reg)		SYSCON_READ_4((sc)->syscon, (reg))
#define	WR4(sc, reg, mask, val)						\
    SYSCON_WRITE_4((sc)->syscon, (reg), ((mask) << GRF_HIWORD_SHIFT) | (val))

#define	MAX_LANE	4

static void
cfg_write(struct rk_pcie_phy_softc *sc, uint32_t reg, uint32_t data)
{
	/* setup register address and data first */
	WR4(sc, GRF_SOC_CON8, 0x7FF,
	    (reg & 0x3F) << 1 | (data & 0x0F) << 7);
	/* dummy readback for sync */
	RD4(sc, GRF_SOC_CON8);

	/* Do write pulse */
	WR4(sc, GRF_SOC_CON8, 1, 1);
	RD4(sc, GRF_SOC_CON8);
	DELAY(10);
	WR4(sc, GRF_SOC_CON8, 1, 0);
	RD4(sc, GRF_SOC_CON8);
	DELAY(10);
}

static uint32_t
cfg_read(struct rk_pcie_phy_softc *sc, uint32_t reg)
{
	uint32_t val;

	WR4(sc, GRF_SOC_CON8, 0x3FF, reg << 1);
	RD4(sc, GRF_SOC_CON8);
	DELAY(10);
	val = RD4(sc, GRF_SOC_STATUS1);
	return ((val >> 8) & 0x0f);
}

static int
rk_pcie_phy_up(struct rk_pcie_phy_softc *sc, int id)
{
	uint32_t val;
	int i, rv;

	PHY_LOCK(sc);

	sc->enable_count++;
	if (sc->enable_count != 1) {
		PHY_UNLOCK(sc);
		return (0);
	}

	rv = hwreset_deassert(sc->hwreset_phy);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot deassert 'phy' reset\n");
		PHY_UNLOCK(sc);
		return (rv);
	}
	/* Un-idle all lanes */
	for (i = 0; i < MAX_LANE; i++)
		WR4(sc, GRF_SOC_CON_5_PCIE, CON_5_PCIE_IDLE_OFF(i), 0);

	/* Wait for PLL lock */
	for (i = 100; i > 0; i--) {
		val = cfg_read(sc, PHY_CFG_PLL_LOCK);
		if (val & CLK_PLL_LOCKED)
			break;
		DELAY(1000);
	}
	if (i <= 0) {
		device_printf(sc->dev, "PLL lock timeouted, 0x%02X\n", val);
		PHY_UNLOCK(sc);
		return (ETIMEDOUT);
	}
	/* Switch PLL to stable 5GHz, rate adjustment is done by divider */
	cfg_write(sc, PHY_CFG_CLK_TEST, CLK_TEST_SEPE_RATE);
	/* Enable 100MHz output for PCIe ref clock */
	cfg_write(sc, PHY_CFG_CLK_SCC, CLK_SCC_PLL_100M);

	/* Wait for ungating of ref clock */
	for (i = 100; i > 0; i--) {
		val = cfg_read(sc, PHY_CFG_SCC_LOCK);
		if ((val & CLK_SCC_100M_GATE) == 0)
			break;
		DELAY(1000);
	}
	if (i <= 0) {
		device_printf(sc->dev, "PLL output enable timeouted\n");
		PHY_UNLOCK(sc);
		return (ETIMEDOUT);
	}

	/* Wait for PLL relock (to 5GHz) */
	for (i = 100; i > 0; i--) {
		val = cfg_read(sc, PHY_CFG_PLL_LOCK);
		if (val & CLK_PLL_LOCKED)
			break;
		DELAY(1000);
	}
	if (i <= 0) {
		device_printf(sc->dev, "PLL relock timeouted\n");
		PHY_UNLOCK(sc);
		return (ETIMEDOUT);
	}

	PHY_UNLOCK(sc);
	return (rv);
}

static int
rk_pcie_phy_down(struct rk_pcie_phy_softc *sc, int id)
{
	int rv;

	PHY_LOCK(sc);

	rv = 0;
	if (sc->enable_count <= 0)
		panic("unpaired enable/disable");

	sc->enable_count--;

	/* Idle given lane */
	WR4(sc, GRF_SOC_CON_5_PCIE,
	    CON_5_PCIE_IDLE_OFF(id),
	    CON_5_PCIE_IDLE_OFF(id));

	if (sc->enable_count == 0) {
		rv = hwreset_assert(sc->hwreset_phy);
		if (rv != 0)
			device_printf(sc->dev, "Cannot assert 'phy' reset\n");
	}
	PHY_UNLOCK(sc);
	return (rv);
}

static int
rk_pcie_phy_enable(struct phynode *phynode, bool enable)
{
	struct rk_pcie_phy_softc *sc;
	device_t dev;
	intptr_t phy;
	int rv;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (enable)
		rv = rk_pcie_phy_up(sc, (int)phy);
	 else
		rv = rk_pcie_phy_down(sc, (int) phy);

	return (rv);
}

/* Phy class and methods. */
static phynode_method_t rk_pcie_phy_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable,		 rk_pcie_phy_enable),

	PHYNODEMETHOD_END
};

DEFINE_CLASS_1( rk_pcie_phy_phynode, rk_pcie_phy_phynode_class,
    rk_pcie_phy_phynode_methods, 0, phynode_class);

static int
 rk_pcie_phy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Rockchip RK3399 PCIe PHY");
	return (BUS_PROBE_DEFAULT);
}

static int
 rk_pcie_phy_attach(device_t dev)
{
	struct rk_pcie_phy_softc *sc;
	struct phynode_init_def phy_init;
	struct phynode *phynode;
	phandle_t node;
	int i, rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);
	PHY_LOCK_INIT(sc);

	if (SYSCON_GET_HANDLE(sc->dev, &sc->syscon) != 0 ||
	    sc->syscon == NULL) {
		device_printf(dev, "cannot get syscon for device\n");
		rv = ENXIO;
		goto fail;
	}

	rv = clk_set_assigned(dev, ofw_bus_get_node(dev));
	if (rv != 0 && rv != ENOENT) {
		device_printf(dev, "clk_set_assigned failed: %d\n", rv);
		rv = ENXIO;
		goto fail;
	}

	rv = clk_get_by_ofw_name(sc->dev, 0, "refclk", &sc->clk_ref);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'refclk' clock\n");
		rv = ENXIO;
		goto fail;
	}
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "phy", &sc->hwreset_phy);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'phy' reset\n");
		rv = ENXIO;
		goto fail;
	}

	rv = hwreset_assert(sc->hwreset_phy);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'phy' reset\n");
		rv = ENXIO;
		goto fail;
	}

	rv = clk_enable(sc->clk_ref);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'ref' clock\n");
		rv = ENXIO;
		goto fail;
	}

	for (i = 0; i < MAX_LANE; i++) {
		phy_init.id = i;
		phy_init.ofw_node = node;
		phynode = phynode_create(dev, &rk_pcie_phy_phynode_class,
		&phy_init);
		if (phynode == NULL) {
			device_printf(dev, "Cannot create phy[%d]\n", i);
			rv = ENXIO;
			goto fail;
		}
		if (phynode_register(phynode) == NULL) {
			device_printf(dev, "Cannot register phy[%d]\n", i);
			rv = ENXIO;
			goto fail;
		}
	}

	return (0);

fail:
	return (rv);
}

static device_method_t rk_pcie_phy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		 rk_pcie_phy_probe),
	DEVMETHOD(device_attach,	 rk_pcie_phy_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(rk_pcie_phy, rk_pcie_phy_driver, rk_pcie_phy_methods,
    sizeof(struct rk_pcie_phy_softc));

EARLY_DRIVER_MODULE(rk_pcie_phy, simplebus, rk_pcie_phy_driver, NULL, NULL,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
