/*
 * Copyright (c) 2026 Bojan Novković <bnovkov@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Driver for the SpacemiT K1 SoC APMU clock control unit.
 *
 * Written using SpacemiT's K1 Chip Product Documentation,
 * Section 9., "Top System (1/2)" and the accompanying clock
 * tree schematic, available at https://www.spacemit.com/community/document/
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>

#include <dt-bindings/clock/spacemit,k1-syscon.h>

#include "k1_clk.h"

#define K1_SDH0_CLK_RES_CTRL 0x54
#define K1_SDH1_CLK_RES_CTRL 0x58
#define K1_SDH2_CLK_RES_CTRL 0xE0
#define K1_PMUA_ACLK_CTRL    0x388

static const struct k1_clk_def apmu_clks[] = {
K1_CLK_DIV_MUX(pmua_aclk, CLK_PMUA_ACLK, K1_PMUA_ACLK_CTRL,
	1, 0, 2, 1, 4,
	"pll1_d10_aud", "pll1_d8"),

K1_CLK_GATE(sdh_axi_aclk, CLK_SDH_AXI, K1_SDH0_CLK_RES_CTRL, 4,
	"pmua_aclk"),

K1_CLK_DIV_MUX_GATE(sdh0_clk, CLK_SDH0, K1_SDH0_CLK_RES_CTRL,
	3, 8, 3, 5, 4, 11,
	"pll1_d6", "pll1_d4", "pll2_d8", "pll2_d5",
	"pll1_d11", "pll1_d11" , "pll1_d13", "pll1_d23"),

K1_CLK_DIV_MUX_GATE(sdh1_clk, CLK_SDH1, K1_SDH1_CLK_RES_CTRL,
	3, 8, 3, 5, 4, 11,
	"pll1_d6", "pll1_d4", "pll2_d8", "pll2_d5",
	"pll1_d11", "pll1_d11" , "pll1_d13", "pll1_d23"),

K1_CLK_DIV_MUX_GATE(sdh2_clk, CLK_SDH2, K1_SDH2_CLK_RES_CTRL,
	3, 8, 3, 5, 4, 11,
	"pll1_d6", "pll1_d4", "pll2_d8", "pll1_d3",
	"pll1_d11", "pll1_d11" , "pll1_d13", "pll1_d23")
};

static const struct k1_reset_def apmu_resets[] = {
K1_RESET(RESET_SDH_AXI, K1_SDH0_CLK_RES_CTRL, 0, 3),
K1_RESET(RESET_SDH0, K1_SDH0_CLK_RES_CTRL, 0, 1),
K1_RESET(RESET_SDH1, K1_SDH1_CLK_RES_CTRL, 0, 1),
K1_RESET(RESET_SDH2, K1_SDH2_CLK_RES_CTRL, 0, 1),
};

#define K1_APMU_NRESETS (RESET_SDH2 + 1);

static int
k1_apmu_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_is_compatible(dev, "spacemit,k1-syscon-apmu"))
		return (ENXIO);
	device_set_desc(dev, "SpacemitT K1 APMU Clock Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
k1_apmu_attach(device_t dev)
{
	struct k1_clkdev_softc *sc;

	sc = device_get_softc(dev);
	sc->clks = apmu_clks;
	sc->nclks = nitems(apmu_clks);

	sc->resets = apmu_resets;
	sc->nresets = K1_APMU_NRESETS;

	return (k1_clk_attach(dev));
}

static device_method_t k1_apmu_clkdev_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	k1_apmu_probe),
	DEVMETHOD(device_attach, 	k1_apmu_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(k1_apmu, k1_apmu_driver, k1_apmu_clkdev_methods,
    sizeof(struct k1_clkdev_softc), k1_clkdev_driver);
EARLY_DRIVER_MODULE(k1_apmu, simplebus, k1_apmu_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LAST);
