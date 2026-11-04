/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MediaTek MT7986 APMIXEDSYS: the PLLs, all fed by the 40 MHz crystal.
 * NET2PLL, MMPLL, WEDMCUPLL, NET1PLL and MPLL also have a reset bar bit
 * (CON0 bit 24) that is released once the PLL has locked.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/mediatek/mtk_clk.h>

#include <dt-bindings/clock/mt7986-clk.h>

#define	MT7986_RST_BAR		(1u << 24)

#define	MT7986_PLL(_id, _name, _con0, _rst_bar, _flags)			\
	{ .type = MTK_CLK_PLL, .pll = {					\
	    .clkdef = { .name = (_name), .id = (_id),			\
		.parent_names = (const char *[]){ "clkxtal" },		\
		.parent_cnt = 1, .flags = (_flags) },			\
	    .con0 = (_con0), .rst_bar = (_rst_bar) } }

static struct mtk_clk_def mt7986_apmixedsys_clks[] = {
	/* 40 MHz crystal, provided by a fixed-clock node */
	MTK_LINK("clkxtal"),

	/*
	 * MPLL, NET1PLL and NET2PLL feed the DRAM, bus and 26 MHz clocks: none
	 * of them may ever stop.  ARMPLL clocks the CPUs through a mux in
	 * MCUSYS, where a cpufreq driver moves them to another clock while it
	 * changes the rate.
	 */
	MT7986_PLL(CLK_APMIXED_ARMPLL,	"armpll",	0x200, 0, 0),
	MT7986_PLL(CLK_APMIXED_NET2PLL,	"net2pll",	0x210, MT7986_RST_BAR,
	    CLK_NODE_CANNOT_STOP),
	MT7986_PLL(CLK_APMIXED_MMPLL,	"mmpll",	0x220, MT7986_RST_BAR,
	    0),
	MT7986_PLL(CLK_APMIXED_SGMPLL,	"sgmpll",	0x230, 0, 0),
	MT7986_PLL(CLK_APMIXED_WEDMCUPLL, "wedmcupll",	0x240, MT7986_RST_BAR,
	    0),
	MT7986_PLL(CLK_APMIXED_NET1PLL,	"net1pll",	0x250, MT7986_RST_BAR,
	    CLK_NODE_CANNOT_STOP),
	MT7986_PLL(CLK_APMIXED_MPLL,	"mpll",		0x260, MT7986_RST_BAR,
	    CLK_NODE_CANNOT_STOP),
	MT7986_PLL(CLK_APMIXED_APLL2,	"apll2",	0x278, 0, 0),
};

static const struct mtk_clk_desc mt7986_apmixedsys_desc = {
	.clks = mt7986_apmixedsys_clks,
	.nclks = nitems(mt7986_apmixedsys_clks),
};

static struct ofw_compat_data compat_data[] = {
	{ "mediatek,mt7986-apmixedsys",	1 },
	{ NULL,				0 }
};

static int
mt7986_apmixedsys_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "MediaTek MT7986 PLL controller");
	return (BUS_PROBE_DEFAULT);
}

static int
mt7986_apmixedsys_attach(device_t dev)
{
	return (mtk_clk_attach(dev, &mt7986_apmixedsys_desc));
}

static device_method_t mt7986_apmixedsys_methods[] = {
	DEVMETHOD(device_probe,		mt7986_apmixedsys_probe),
	DEVMETHOD(device_attach,	mt7986_apmixedsys_attach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(mt7986_apmixedsys, mt7986_apmixedsys_driver,
    mt7986_apmixedsys_methods, sizeof(struct mtk_clk_softc), mtk_clk_driver);

/* After the fixed clocks, so the ARMPLL reference reaches the crystal. */
EARLY_DRIVER_MODULE(mt7986_apmixedsys, simplebus, mt7986_apmixedsys_driver,
    NULL, NULL, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 1);
