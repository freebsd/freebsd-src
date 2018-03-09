/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm64/rockchip/clk/rk_cru.h>

/* GATES */

#define ACLK_PERI		153
#define HCLK_SDMMC		317
#define HCLK_SDIO		318
#define HCLK_EMMC		319
#define HCLK_SDMMC_EXT		320

static struct rk_cru_gate rk3328_gates[] = {
	/* CRU_CLKGATE_CON0 */
	CRU_GATE(0, "apll_core", "apll", 0x200, 0)
	CRU_GATE(0, "dpll_core", "dpll", 0x200, 1)
	CRU_GATE(0, "gpll_core", "gpll", 0x200, 2)
	CRU_GATE(0, "npll_core", "npll", 0x200, 12)

	/* CRU_CLKGATE_CON4 */
	CRU_GATE(0, "gpll_peri", "gpll", 0x210, 0)
	CRU_GATE(0, "cpll_peri", "cpll", 0x210, 1)

	/* CRU_CLKGATE_CON8 */
	CRU_GATE(0, "pclk_bus", "pclk_bus_pre", 0x220, 3)
	CRU_GATE(0, "pclk_phy_pre", "pclk_bus_pre", 0x220, 4)

	/* CRU_CLKGATE_CON10 */
	CRU_GATE(ACLK_PERI, "aclk_peri", "aclk_peri_pre", 0x228, 0)

	/* CRU_CLKGATE_CON19 */
	CRU_GATE(HCLK_SDMMC, "hclk_sdmmc", "hclk_peri", 0x24C, 0)
	CRU_GATE(HCLK_SDIO, "hclk_sdio", "hclk_peri", 0x24C, 1)
	CRU_GATE(HCLK_EMMC, "hclk_emmc", "hclk_peri", 0x24C, 2)
	CRU_GATE(HCLK_SDMMC_EXT, "hclk_sdmmc_ext", "hclk_peri", 0x24C, 15)
};

/*
 * PLLs
 */

#define PLL_APLL		1
#define PLL_DPLL		2
#define PLL_CPLL		3
#define PLL_GPLL		4
#define PLL_NPLL		5

static const char *pll_parents[] = {"xin24m"};
static struct rk_clk_pll_def apll = {
	.clkdef = {
		.id = PLL_APLL,
		.name = "apll",
		.parent_names = pll_parents,
		.parent_cnt = nitems(pll_parents),
	},
	.base_offset = 0x00,
	.gate_offset = 0x200,
	.gate_shift = 0,
	.flags = RK_CLK_PLL_HAVE_GATE,
};

static struct rk_clk_pll_def dpll = {
	.clkdef = {
		.id = PLL_DPLL,
		.name = "dpll",
		.parent_names = pll_parents,
		.parent_cnt = nitems(pll_parents),
	},
	.base_offset = 0x20,
	.gate_offset = 0x200,
	.gate_shift = 1,
	.flags = RK_CLK_PLL_HAVE_GATE,
};

static struct rk_clk_pll_def cpll = {
	.clkdef = {
		.id = PLL_CPLL,
		.name = "cpll",
		.parent_names = pll_parents,
		.parent_cnt = nitems(pll_parents),
	},
	.base_offset = 0x40,
};

static struct rk_clk_pll_def gpll = {
	.clkdef = {
		.id = PLL_GPLL,
		.name = "gpll",
		.parent_names = pll_parents,
		.parent_cnt = nitems(pll_parents),
	},
	.base_offset = 0x60,
	.gate_offset = 0x200,
	.gate_shift = 2,
	.flags = RK_CLK_PLL_HAVE_GATE,
};

static struct rk_clk_pll_def npll = {
	.clkdef = {
		.id = PLL_NPLL,
		.name = "npll",
		.parent_names = pll_parents,
		.parent_cnt = nitems(pll_parents),
	},
	.base_offset = 0xa0,
	.gate_offset = 0x200,
	.gate_shift = 12,
	.flags = RK_CLK_PLL_HAVE_GATE,
};

/* CRU_CLKSEL_CON0 */
#define ACLK_BUS_PRE		136

/* Needs hdmiphy as parent too*/
static const char *aclk_bus_pre_parents[] = {"cpll", "gpll"};
static struct rk_clk_composite_def aclk_bus_pre = {
	.clkdef = {
		.id = ACLK_BUS_PRE,
		.name = "aclk_bus_pre",
		.parent_names = aclk_bus_pre_parents,
		.parent_cnt = nitems(aclk_bus_pre_parents),
	},
	.muxdiv_offset = 0x100,
	.mux_shift = 13,
	.mux_width = 2,

	.div_shift = 8,
	.div_width = 5,

	.gate_offset = 0x232,
	.gate_shift = 0,

	.flags = RK_CLK_COMPOSITE_HAVE_MUX | RK_CLK_COMPOSITE_HAVE_GATE,
};

/* CRU_CLKSEL_CON1 */

#define PCLK_BUS_PRE		216
#define HCLK_BUS_PRE		328

static const char *hclk_bus_pre_parents[] = {"aclk_bus_pre"};
static struct rk_clk_composite_def hclk_bus_pre = {
	.clkdef = {
		.id = HCLK_BUS_PRE,
		.name = "hclk_bus_pre",
		.parent_names = hclk_bus_pre_parents,
		.parent_cnt = nitems(hclk_bus_pre_parents),
	},
	.muxdiv_offset = 0x104,

	.div_shift = 8,
	.div_width = 2,

	.gate_offset = 0x232,
	.gate_shift = 1,

	.flags = RK_CLK_COMPOSITE_HAVE_GATE,
};

static const char *pclk_bus_pre_parents[] = {"aclk_bus_pre"};
static struct rk_clk_composite_def pclk_bus_pre = {
	.clkdef = {
		.id = PCLK_BUS_PRE,
		.name = "pclk_bus_pre",
		.parent_names = pclk_bus_pre_parents,
		.parent_cnt = nitems(pclk_bus_pre_parents),
	},
	.muxdiv_offset = 0x104,

	.div_shift = 12,
	.div_width = 3,

	.gate_offset = 0x232,
	.gate_shift = 2,

	.flags = RK_CLK_COMPOSITE_HAVE_GATE,
};

/* CRU_CLKSEL_CON28 */

#define ACLK_PERI_PRE		137

static const char *aclk_peri_pre_parents[] = {"cpll", "gpll"/* , "hdmiphy" */};
static struct rk_clk_composite_def aclk_peri_pre = {
	.clkdef = {
		.id = ACLK_PERI_PRE,
		.name = "aclk_peri_pre",
		.parent_names = aclk_peri_pre_parents,
		.parent_cnt = nitems(aclk_peri_pre_parents),
	},
	.muxdiv_offset = 0x170,

	.mux_shift = 6,
	.mux_width = 2,

	.div_shift = 0,
	.div_width = 5,

	.flags = RK_CLK_COMPOSITE_HAVE_MUX,
};

/* CRU_CLKSEL_CON29 */

#define PCLK_PERI		230
#define HCLK_PERI		308

static const char *phclk_peri_parents[] = {"aclk_peri_pre"};
static struct rk_clk_composite_def pclk_peri = {
	.clkdef = {
		.id = PCLK_PERI,
		.name = "pclk_peri",
		.parent_names = phclk_peri_parents,
		.parent_cnt = nitems(phclk_peri_parents),
	},

	.div_shift = 0,
	.div_width = 2,

	/* CRU_CLKGATE_CON10 */
	.gate_offset = 0x228,
	.gate_shift = 2,

	.flags = RK_CLK_COMPOSITE_HAVE_GATE,
};

static struct rk_clk_composite_def hclk_peri = {
	.clkdef = {
		.id = HCLK_PERI,
		.name = "hclk_peri",
		.parent_names = phclk_peri_parents,
		.parent_cnt = nitems(phclk_peri_parents),
	},

	.div_shift = 4,
	.div_width = 3,

	/* CRU_CLKGATE_CON10 */
	.gate_offset = 0x228,
	.gate_shift = 1,

	.flags = RK_CLK_COMPOSITE_HAVE_GATE,
};

/* CRU_CLKSEL_CON30 */

#define SCLK_SDMMC		33

static const char *mmc_parents[] = {"cpll", "gpll", "xin24m"/* , "usb480m" */};
static struct rk_clk_composite_def sdmmc = {
	.clkdef = {
		.id = SCLK_SDMMC,
		.name = "clk_sdmmc",
		.parent_names = mmc_parents,
		.parent_cnt = nitems(mmc_parents),
	},
	.muxdiv_offset = 0x178,

	.mux_shift = 8,
	.mux_width = 2,

	.div_shift = 0,
	.div_width = 8,

	/* CRU_CLKGATE_CON4 */
	.gate_offset = 0x210,
	.gate_shift = 3,

	.flags = RK_CLK_COMPOSITE_HAVE_MUX | RK_CLK_COMPOSITE_HAVE_GATE,
};

/* CRU_CLKSEL_CON31 */
#define SCLK_SDIO		34

static struct rk_clk_composite_def sdio = {
	.clkdef = {
		.id = SCLK_SDIO,
		.name = "clk_sdio",
		.parent_names = mmc_parents,
		.parent_cnt = nitems(mmc_parents),
	},
	.muxdiv_offset = 0x17C,

	.mux_shift = 8,
	.mux_width = 2,

	.div_shift = 0,
	.div_width = 8,

	/* CRU_CLKGATE_CON4 */
	.gate_offset = 0x210,
	.gate_shift = 4,

	.flags = RK_CLK_COMPOSITE_HAVE_MUX | RK_CLK_COMPOSITE_HAVE_GATE,
};

/* CRU_CLKSEL_CON32 */
#define SCLK_EMMC		35

static struct rk_clk_composite_def emmc = {
	.clkdef = {
		.id = SCLK_EMMC,
		.name = "clk_emmc",
		.parent_names = mmc_parents,
		.parent_cnt = nitems(mmc_parents),
	},
	.muxdiv_offset = 0x180,

	.mux_shift = 8,
	.mux_width = 2,

	.div_shift = 0,
	.div_width = 8,

	/* CRU_CLKGATE_CON4 */
	.gate_offset = 0x210,
	.gate_shift = 5,

	.flags = RK_CLK_COMPOSITE_HAVE_MUX | RK_CLK_COMPOSITE_HAVE_GATE,
};

static struct rk_clk rk3328_clks[] = {
	[PLL_APLL] = {
		.type = RK_CLK_PLL,
		.clk.pll = &apll
	},
	[PLL_DPLL] = {
		.type = RK_CLK_PLL,
		.clk.pll = &dpll
	},
	[PLL_CPLL] = {
		.type = RK_CLK_PLL,
		.clk.pll = &cpll
	},
	[PLL_GPLL] = {
		.type = RK_CLK_PLL,
		.clk.pll = &gpll
	},
	[PLL_NPLL] = {
		.type = RK_CLK_PLL,
		.clk.pll = &npll
	},

	[ACLK_BUS_PRE] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &aclk_bus_pre
	},
	[HCLK_BUS_PRE] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &hclk_bus_pre
	},
	[PCLK_BUS_PRE] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &pclk_bus_pre
	},

	[ACLK_PERI_PRE] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &aclk_peri_pre,
	},
	[PCLK_PERI] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &pclk_peri,
	},
	[HCLK_PERI] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &hclk_peri,
	},
	[SCLK_SDMMC] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &sdmmc
	},
	[SCLK_SDIO] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &sdio
	},
	[SCLK_EMMC] = {
		.type = RK_CLK_COMPOSITE,
		.clk.composite = &emmc
	},
};

static int
rk3328_cru_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "rockchip,rk3328-cru")) {
		device_set_desc(dev, "Rockchip RK3328 Clock and Reset Unit");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
rk3328_cru_attach(device_t dev)
{
	struct rk_cru_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->gates = rk3328_gates;
	sc->ngates = nitems(rk3328_gates);

	sc->clks = rk3328_clks;
	sc->nclks = nitems(rk3328_clks);

	return (rk_cru_attach(dev));
}

static device_method_t rk3328_cru_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk3328_cru_probe),
	DEVMETHOD(device_attach,	rk3328_cru_attach),

	DEVMETHOD_END
};

static devclass_t rk3328_cru_devclass;

DEFINE_CLASS_1(rk3328_cru, rk3328_cru_driver, rk3328_cru_methods,
  sizeof(struct rk_cru_softc), rk_cru_driver);

EARLY_DRIVER_MODULE(rk3328_cru, simplebus, rk3328_cru_driver,
    rk3328_cru_devclass, 0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
