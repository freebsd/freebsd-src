/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk_fixed.h>

#include <arm64/qoriq/clk/qoriq_clkgen.h>

static uint8_t ls1046a_pltfrm_pll_divs[] = {
	2, 4, 0
};

static struct qoriq_clk_pll_def ls1046a_pltfrm_pll = {
	.clkdef = {
		.name = "ls1046a_platform_pll",
		.id = QORIQ_CLK_ID(QORIQ_TYPE_PLATFORM_PLL, 0),
		.flags = 0
	},
	.offset = 0xC00,
	.shift = 1,
	.mask = 0x7E,
	.dividers = ls1046a_pltfrm_pll_divs,
	.flags = 0
};

static const uint8_t ls1046a_cga1_pll_divs[] = {
	2, 3, 4, 0
};

static struct qoriq_clk_pll_def ls1046a_cga1_pll = {
	.clkdef = {
		.name = "ls1046a_cga_pll1",
		.id = QORIQ_CLK_ID(QORIQ_TYPE_INTERNAL, 0),
		.flags = 0
	},
	.offset = 0x800,
	.shift = 1,
	.mask = 0x1FE,
	.dividers = ls1046a_cga1_pll_divs,
	.flags = QORIQ_CLK_PLL_HAS_KILL_BIT
};

static struct qoriq_clk_pll_def ls1046a_cga2_pll = {
	.clkdef = {
		.name = "ls1046a_cga_pll2",
		.id = QORIQ_CLK_ID(QORIQ_TYPE_INTERNAL, 20),
		.flags = 0
	},
	.offset = 0x820,
	.shift = 1,
	.mask = 0x1FE,
	.dividers = ls1046a_cga1_pll_divs,
	.flags = QORIQ_CLK_PLL_HAS_KILL_BIT
};

static struct qoriq_clk_pll_def *ls1046a_cga_plls[] = {
	&ls1046a_cga1_pll,
	&ls1046a_cga2_pll
};

static const char *ls1046a_cmux0_parent_names[] = {
	"ls1046a_cga_pll1",
	NULL,
	"ls1046a_cga_pll1_div2",
	NULL,
	"ls1046a_cga_pll2",
	NULL,
	"ls1046a_cga_pll2_div2"
};

static struct clk_mux_def ls1046a_cmux0 = {
	.clkdef = {
		.name = "ls1046a_cmux0",
		.id = QORIQ_CLK_ID(QORIQ_TYPE_CMUX, 0),
		.parent_names = ls1046a_cmux0_parent_names,
		.parent_cnt = nitems(ls1046a_cmux0_parent_names),
		.flags = 0
	},
	.offset = 0,
	.shift = 27,
	.width = 4,
	.mux_flags = 0
};

static const char *ls1046a_hwaccel1_parent_names[] = {
	NULL,
	NULL,
	"ls1046a_cga_pll1_div2",
	"ls1046a_cga_pll1_div3",
	"ls1046a_cga_pll1_div4",
	"ls1046a_platform_pll",
	"ls1046a_cga_pll2_div2",
	"ls1046a_cga_pll2_div3"
};

static const char *ls1046a_hwaccel2_parent_names[] = {
	NULL,
	"ls1046a_cga_pll2",
	"ls1046a_cga_pll2_div2",
	"ls1046a_cga_pll2_div3",
	NULL,
	NULL,
	"ls1046a_cga_pll1_div2"
};

static struct clk_mux_def ls1046a_hwaccel1 = {
	.clkdef = {
		.name = "ls1046a_hwaccel1",
		.id = QORIQ_CLK_ID(QORIQ_TYPE_HWACCEL, 0),
		.parent_names = ls1046a_hwaccel1_parent_names,
		.parent_cnt = nitems(ls1046a_hwaccel1_parent_names),
		.flags = 0
	},
	.offset = 0x10,
	.shift = 27,
	.width = 4,
	.mux_flags = 0
};

static struct clk_mux_def ls1046a_hwaccel2 = {
	.clkdef = {
		.name = "ls1046a_hwaccel2",
		.id = QORIQ_CLK_ID(QORIQ_TYPE_HWACCEL, 1),
		.parent_names = ls1046a_hwaccel2_parent_names,
		.parent_cnt = nitems(ls1046a_hwaccel2_parent_names),
		.flags = 0
	},
	.offset = 0x30,
	.shift = 27,
	.width = 4,
	.mux_flags = 0
};

static struct clk_mux_def *ls1046a_mux_nodes[] = {
	&ls1046a_cmux0,
	&ls1046a_hwaccel1,
	&ls1046a_hwaccel2
};

const char *ls1046a_fman_srcs[] = {
	"ls1046a_hwaccel1"
};

static int ls1046a_clkgen_probe(device_t);
static int ls1046a_clkgen_attach(device_t);

static device_method_t ls1046a_clkgen_methods[] = {
	DEVMETHOD(device_probe,		ls1046a_clkgen_probe),
	DEVMETHOD(device_attach,	ls1046a_clkgen_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ls1046a_clkgen, ls1046a_clkgen_driver, ls1046a_clkgen_methods,
    sizeof(struct qoriq_clkgen_softc), qoriq_clkgen_driver);

EARLY_DRIVER_MODULE(ls1046a_clkgen, simplebus, ls1046a_clkgen_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);

static int
ls1046a_fman_init(device_t dev)
{
	struct qoriq_clkgen_softc *sc;
	struct clk_fixed_def def;
	int error;

	sc = device_get_softc(dev);

	def.clkdef.name = "ls1046a_fman",
	def.clkdef.id = QORIQ_CLK_ID(QORIQ_TYPE_FMAN, 0),
	def.clkdef.parent_names = ls1046a_fman_srcs;
	def.clkdef.parent_cnt = nitems(ls1046a_fman_srcs);
	def.clkdef.flags = 0;
	def.freq = 0;
	def.mult = 1;
	def.div = 1;
	def.fixed_flags = 0;

	error = clknode_fixed_register(sc->clkdom, &def);
	return (error);
}

static int
ls1046a_clkgen_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if(!ofw_bus_is_compatible(dev, "fsl,ls1046a-clockgen"))
		return (ENXIO);

	device_set_desc(dev, "LS1046A clockgen");
	return (BUS_PROBE_DEFAULT);
}

static int
ls1046a_clkgen_attach(device_t dev)
{
	struct qoriq_clkgen_softc *sc;

	sc = device_get_softc(dev);

	sc->pltfrm_pll_def = &ls1046a_pltfrm_pll;
	sc->cga_pll = ls1046a_cga_plls;
	sc->cga_pll_num = nitems(ls1046a_cga_plls);
	sc->mux = ls1046a_mux_nodes;
	sc->mux_num = nitems(ls1046a_mux_nodes);
	sc->init_func = ls1046a_fman_init;
	sc->flags = 0;

	return (qoriq_clkgen_attach(dev));
}
