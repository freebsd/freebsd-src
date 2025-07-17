/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
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
 */

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

#ifdef __aarch64__
#include "opt_soc.h"
#endif

#include <dev/clk/clk_div.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_mux.h>

#include <dev/hwreset/hwreset.h>

#include <dev/clk/allwinner/aw_ccung.h>

#include <dt-bindings/clock/sun8i-de2.h>
#include <dt-bindings/reset/sun8i-de2.h>

enum CCU_DE2 {
	H3_CCU = 1,
	A64_CCU,
};

/* Non exported clocks */
#define	CLK_MIXER0_DIV	3
#define	CLK_MIXER1_DIV	4
#define	CLK_WB_DIV	5

static struct aw_ccung_reset h3_de2_ccu_resets[] = {
	CCU_RESET(RST_MIXER0, 0x08, 0)
	CCU_RESET(RST_WB, 0x08, 2)
};

static struct aw_ccung_reset a64_de2_ccu_resets[] = {
	CCU_RESET(RST_MIXER0, 0x08, 0)
	CCU_RESET(RST_MIXER1, 0x08, 1)
	CCU_RESET(RST_WB, 0x08, 2)
};

static struct aw_ccung_gate h3_de2_ccu_gates[] = {
	CCU_GATE(CLK_BUS_MIXER0, "mixer0", "mixer0-div", 0x00, 0)
	CCU_GATE(CLK_BUS_WB, "wb", "wb-div", 0x00, 2)

	CCU_GATE(CLK_MIXER0, "bus-mixer0", "bus-de", 0x04, 0)
	CCU_GATE(CLK_WB, "bus-wb", "bus-de", 0x04, 2)
};

static struct aw_ccung_gate a64_de2_ccu_gates[] = {
	CCU_GATE(CLK_BUS_MIXER0, "mixer0", "mixer0-div", 0x00, 0)
	CCU_GATE(CLK_BUS_MIXER1, "mixer1", "mixer1-div", 0x00, 1)
	CCU_GATE(CLK_BUS_WB, "wb", "wb-div", 0x00, 2)

	CCU_GATE(CLK_MIXER0, "bus-mixer0", "bus-de", 0x04, 0)
	CCU_GATE(CLK_MIXER1, "bus-mixer1", "bus-de", 0x04, 1)
	CCU_GATE(CLK_WB, "bus-wb", "bus-de", 0x04, 2)
};

static const char *div_parents[] = {"de"};

NM_CLK(mixer0_div_clk,
    CLK_MIXER0_DIV,			/* id */
    "mixer0-div", div_parents,		/* names, parents */
    0x0C,				/* offset */
    0, 0, 1, AW_CLK_FACTOR_FIXED,	/* N factor (fake)*/
    0, 4, 0, 0,				/* M flags */
    0, 0,				/* mux */
    0,					/* gate */
    AW_CLK_SCALE_CHANGE);	/* flags */

NM_CLK(mixer1_div_clk,
    CLK_MIXER1_DIV,			/* id */
    "mixer1-div", div_parents,		/* names, parents */
    0x0C,				/* offset */
    0, 0, 1, AW_CLK_FACTOR_FIXED,	/* N factor (fake)*/
    4, 4, 0, 0,				/* M flags */
    0, 0,				/* mux */
    0,					/* gate */
    AW_CLK_SCALE_CHANGE);		/* flags */

NM_CLK(wb_div_clk,
    CLK_WB_DIV,				/* id */
    "wb-div", div_parents,		/* names, parents */
    0x0C,				/* offset */
    0, 0, 1, AW_CLK_FACTOR_FIXED,	/* N factor (fake)*/
    8, 4, 0, 0,				/* M flags */
    0, 0,				/* mux */
    0,					/* gate */
    AW_CLK_SCALE_CHANGE);		/* flags */

static struct aw_ccung_clk h3_de2_ccu_clks[] = {
	{ .type = AW_CLK_NM, .clk.nm = &mixer0_div_clk},
	{ .type = AW_CLK_NM, .clk.nm = &wb_div_clk},
};

static struct aw_ccung_clk a64_de2_ccu_clks[] = {
	{ .type = AW_CLK_NM, .clk.nm = &mixer0_div_clk},
	{ .type = AW_CLK_NM, .clk.nm = &mixer1_div_clk},
	{ .type = AW_CLK_NM, .clk.nm = &wb_div_clk},
};

static struct ofw_compat_data compat_data[] = {
	{"allwinner,sun8i-h3-de2-clk", H3_CCU},
	{"allwinner,sun50i-a64-de2-clk", A64_CCU},
	{NULL,             0}
};

static int
ccu_de2_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner DE2 Clock Control Unit");
	return (BUS_PROBE_DEFAULT);
}

static int
ccu_de2_attach(device_t dev)
{
	struct aw_ccung_softc *sc;
	phandle_t node;
	clk_t mod, bus;
	hwreset_t rst_de;
	enum CCU_DE2 type;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	type = (enum CCU_DE2)ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	switch (type) {
	case H3_CCU:
		sc->resets = h3_de2_ccu_resets;
		sc->nresets = nitems(h3_de2_ccu_resets);
		sc->gates = h3_de2_ccu_gates;
		sc->ngates = nitems(h3_de2_ccu_gates);
		sc->clks = h3_de2_ccu_clks;
		sc->nclks = nitems(h3_de2_ccu_clks);
		break;
	case A64_CCU:
		sc->resets = a64_de2_ccu_resets;
		sc->nresets = nitems(a64_de2_ccu_resets);
		sc->gates = a64_de2_ccu_gates;
		sc->ngates = nitems(a64_de2_ccu_gates);
		sc->clks = a64_de2_ccu_clks;
		sc->nclks = nitems(a64_de2_ccu_clks);
		break;
	}

	if (hwreset_get_by_ofw_idx(dev, node, 0, &rst_de) != 0) {
		device_printf(dev, "Cannot get de reset\n");
		return (ENXIO);
	}
	if (hwreset_deassert(rst_de) != 0) {
		device_printf(dev, "Cannot de-assert de reset\n");
		return (ENXIO);
	}

	if (clk_get_by_ofw_name(dev, node, "mod", &mod) != 0) {
		device_printf(dev, "Cannot get mod clock\n");
		return (ENXIO);
	}
	if (clk_enable(mod) != 0) {
		device_printf(dev, "Cannot enable mod clock\n");
		return (ENXIO);
	}

	if (clk_get_by_ofw_name(dev, node, "bus", &bus) != 0) {
		device_printf(dev, "Cannot get bus clock\n");
		return (ENXIO);
	}
	if (clk_enable(bus) != 0) {
		device_printf(dev, "Cannot enable bus clock\n");
		return (ENXIO);
	}

	return (aw_ccung_attach(dev));
}

static device_method_t ccu_de2_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ccu_de2_probe),
	DEVMETHOD(device_attach,	ccu_de2_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ccu_de2, ccu_de2_driver, ccu_de2_methods,
  sizeof(struct aw_ccung_softc), aw_ccung_driver);

EARLY_DRIVER_MODULE(ccu_de2, simplebus, ccu_de2_driver, 0, 0,
    BUS_PASS_RESOURCE + BUS_PASS_ORDER_LAST);
