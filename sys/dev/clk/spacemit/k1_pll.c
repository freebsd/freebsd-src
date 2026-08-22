/*
 * Copyright (c) 2026 Bojan Novković <bnovkov@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Driver for the SpacemiT K1 SoC PLL clocks.
 *
 * Written using SpacemiT's K1 Chip Product Documentation,
 * Section 9., "Top System (1/2)" and the accompanying clock
 * tree schematic, available at https://www.spacemit.com/community/document/
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>

#include <dt-bindings/clock/spacemit,k1-syscon.h>

#include "clkdev_if.h"
#include "k1_clk.h"

#define K1_PLL1_SW2_CTRL 0x110
#define K1_PLL2_SW2_CTRL 0x11C
#define K1_PLL3_SW2_CTRL 0x128

#define K1_PLL_ROOT(_clkname, _id, _rate)	\
	{					\
	.clkdef = {				\
		.id = _id,			\
		.name = #_clkname,		\
	},					\
	.rate = _rate,				\
}

#define K1_PLL_DIV(_clkname, _id, _div, ...)				\
	{								\
	.clkdef = {							\
		.id = _id,						\
		.name = #_clkname,					\
		.parent_names = (const char * []) { __VA_ARGS__ },	\
		.parent_cnt = K1_CLK_PARENT_CNT(__VA_ARGS__),		\
	},								\
	.fixed_div = _div,						\
}

#define K1_PLL_DIV_GATE(_clkname, _id, _div, _reg, _gate_bit, ...)	\
	{								\
	.clkdef = {							\
		.id = _id,						\
		.name = #_clkname,					\
		.parent_names = (const char * []) { __VA_ARGS__ },	\
		.parent_cnt = K1_CLK_PARENT_CNT(__VA_ARGS__),		\
	},								\
	.fixed_div = _div,						\
	.reg = _reg,							\
	.gate_mask = (1 << _gate_bit),					\
}

static struct k1_clk_def k1_plls[] = {
    K1_PLL_ROOT(pll1, CLK_PLL1, 2457600000UL),
    K1_PLL_ROOT(pll2, CLK_PLL2, 3000000000UL),
    K1_PLL_ROOT(pll3, CLK_PLL3, 3200000000UL),

    K1_PLL_DIV_GATE(pll1_d2, CLK_PLL1_D2, 2, K1_PLL1_SW2_CTRL, 1, "pll1"),
    K1_PLL_DIV_GATE(pll1_d3, CLK_PLL1_D3, 3, K1_PLL1_SW2_CTRL, 2, "pll1"),
    K1_PLL_DIV_GATE(pll1_d4, CLK_PLL1_D4, 4, K1_PLL1_SW2_CTRL, 3, "pll1"),
    K1_PLL_DIV_GATE(pll1_d5, CLK_PLL1_D5, 5, K1_PLL1_SW2_CTRL, 4, "pll1"),
    K1_PLL_DIV_GATE(pll1_d6, CLK_PLL1_D6, 6, K1_PLL1_SW2_CTRL, 5, "pll1"),
    K1_PLL_DIV_GATE(pll1_d7, CLK_PLL1_D7, 7, K1_PLL1_SW2_CTRL, 6, "pll1"),
    K1_PLL_DIV_GATE(pll1_d8, CLK_PLL1_D8, 8, K1_PLL1_SW2_CTRL, 7, "pll1"),
    K1_PLL_DIV_GATE(pll1_d11, CLK_PLL1_D11, 11, K1_PLL1_SW2_CTRL, 15, "pll1"),
    K1_PLL_DIV_GATE(pll1_d13, CLK_PLL1_D13, 13, K1_PLL1_SW2_CTRL, 16, "pll1"),
    K1_PLL_DIV_GATE(pll1_d23, CLK_PLL1_D23, 23, K1_PLL1_SW2_CTRL, 20, "pll1"),
    K1_PLL_DIV_GATE(pll1_d64, CLK_PLL1_D64, 64, K1_PLL1_SW2_CTRL, 0, "pll1"),
    K1_PLL_DIV_GATE(pll1_d10_aud, CLK_PLL1_D10_AUD, 10, K1_PLL1_SW2_CTRL, 10, "pll1"),
    K1_PLL_DIV_GATE(pll1_d100_aud, CLK_PLL1_D100_AUD, 100, K1_PLL1_SW2_CTRL, 11, "pll1"),

    K1_PLL_DIV_GATE(pll2_d2, CLK_PLL2_D2, 2, K1_PLL2_SW2_CTRL, 1, "pll2"),
    K1_PLL_DIV_GATE(pll2_d3, CLK_PLL2_D3, 3, K1_PLL2_SW2_CTRL, 2, "pll2"),
    K1_PLL_DIV_GATE(pll2_d4, CLK_PLL2_D4, 4, K1_PLL2_SW2_CTRL, 3, "pll2"),
    K1_PLL_DIV_GATE(pll2_d5, CLK_PLL2_D5, 5, K1_PLL2_SW2_CTRL, 4, "pll2"),
    K1_PLL_DIV_GATE(pll2_d6, CLK_PLL2_D6, 6, K1_PLL2_SW2_CTRL, 5, "pll2"),
    K1_PLL_DIV_GATE(pll2_d7, CLK_PLL2_D7, 7, K1_PLL2_SW2_CTRL, 6, "pll2"),
    K1_PLL_DIV_GATE(pll2_d8, CLK_PLL2_D8, 8, K1_PLL2_SW2_CTRL, 7, "pll2"),

    K1_PLL_DIV_GATE(pll3_d2, CLK_PLL3_D2, 2, K1_PLL3_SW2_CTRL, 1, "pll3"),
    K1_PLL_DIV_GATE(pll3_d3, CLK_PLL3_D3, 3, K1_PLL3_SW2_CTRL, 2, "pll3"),
    K1_PLL_DIV_GATE(pll3_d4, CLK_PLL3_D4, 4, K1_PLL3_SW2_CTRL, 3, "pll3"),
    K1_PLL_DIV_GATE(pll3_d5, CLK_PLL3_D5, 5, K1_PLL3_SW2_CTRL, 4, "pll3"),
    K1_PLL_DIV_GATE(pll3_d6, CLK_PLL3_D6, 6, K1_PLL3_SW2_CTRL, 5, "pll3"),
    K1_PLL_DIV_GATE(pll3_d7, CLK_PLL3_D7, 7, K1_PLL3_SW2_CTRL, 6, "pll3"),
    K1_PLL_DIV_GATE(pll3_d8, CLK_PLL3_D8, 8, K1_PLL3_SW2_CTRL, 7, "pll3"),
    K1_PLL_DIV(pll3_80, CLK_PLL3_80, 20, "pll3_d8"),
    K1_PLL_DIV(pll3_40, CLK_PLL3_40, 10, "pll3_d8"),
    K1_PLL_DIV(pll3_20, CLK_PLL3_20, 5, "pll3_d8"),
};

/*
 * The SpacemiT K1 manual states that "Changes of the run-time frequency
 * in the PLL{1,2,3} output are only available for debugging purposes and
 * should not be used in production systems", which is why this driver does
 * not support changing the top-level PLL frequency and instead treats all
 * PLLs as gateable fixed clocks.
 */
static int
k1_pll_recalc_freq(struct clknode *clk, uint64_t *freq)
{
	struct k1_clk_def *sc;

	sc = clknode_get_softc(clk);
	if (sc->fixed_div != 0)
		*freq = *freq / sc->fixed_div;
	else
		*freq = sc->rate;
	return (0);
}

static int
k1_pll_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	struct k1_clk_def *sc;

	sc = clknode_get_softc(clk);
	if (sc->fixed_div == 0) {
		/* Top-level PLL */
		*stop = 1;
		if (*fout != sc->rate)
			return (ERANGE);
		return (0);
	}
	*stop = 0;
	*fout = *fout / sc->fixed_div;

	return (0);
}

static clknode_method_t k1_pll_clknode_methods[] = {
	CLKNODEMETHOD(clknode_set_freq,		k1_pll_set_freq),
	CLKNODEMETHOD(clknode_recalc_freq,	k1_pll_recalc_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(k1_pll_clknode, k1_pll_clknode_class, k1_pll_clknode_methods,
    sizeof(struct k1_clk_def), k1_clknode_class);

static int
k1_pll_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "spacemit,k1-pll"))
		return (ENXIO);
	device_set_desc(dev, "SpacemiT K1 PLL Clock Control Unit");

	return (BUS_PROBE_DEFAULT);
}

static int
k1_pll_attach(device_t dev)
{
	struct k1_clkdev_softc *sc;

	sc = device_get_softc(dev);
	sc->clks = k1_plls;
	sc->nclks = nitems(k1_plls);
	sc->clknode_class = &k1_pll_clknode_class;

	return (k1_clk_attach(dev));
}

static device_method_t k1_pll_methods[] = {
	DEVMETHOD(device_probe,		k1_pll_probe),
	DEVMETHOD(device_attach,	k1_pll_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(k1_pll, k1_pll_driver, k1_pll_methods,
    sizeof(struct k1_clkdev_softc), k1_clkdev_driver);
EARLY_DRIVER_MODULE(k1_pll, simplebus, k1_pll_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_EARLY);
