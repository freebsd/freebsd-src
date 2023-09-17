/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/libkern.h>

#include <machine/bus.h>
#include <dev/fdt/simplebus.h>

#include <dev/extres/clk/clk_div.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/clk/ti_clk_dpll.h>
#include "clock_common.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

/*
 * Devicetree description
 * Documentation/devicetree/bindings/clock/ti/dpll.txt
 */

struct ti_dpll_softc {
	device_t		dev;
	uint8_t			dpll_type;

	bool			attach_done;
	struct ti_clk_dpll_def	dpll_def;

	struct clock_cell_info	clock_cell;
	struct clkdom		*clkdom;
};

static int ti_dpll_probe(device_t dev);
static int ti_dpll_attach(device_t dev);
static int ti_dpll_detach(device_t dev);

#define TI_OMAP3_DPLL_CLOCK			17
#define TI_OMAP3_DPLL_CORE_CLOCK		16
#define TI_OMAP3_DPLL_PER_CLOCK			15
#define TI_OMAP3_DPLL_PER_J_TYPE_CLOCK		14
#define TI_OMAP4_DPLL_CLOCK			13
#define TI_OMAP4_DPLL_X2_CLOCK			12
#define TI_OMAP4_DPLL_CORE_CLOCK		11
#define TI_OMAP4_DPLL_M4XEN_CLOCK		10
#define TI_OMAP4_DPLL_J_TYPE_CLOCK		9
#define TI_OMAP5_MPU_DPLL_CLOCK			8
#define TI_AM3_DPLL_NO_GATE_CLOCK		7
#define TI_AM3_DPLL_J_TYPE_CLOCK		6
#define TI_AM3_DPLL_NO_GATE_J_TYPE_CLOCK	5
#define TI_AM3_DPLL_CLOCK			4
#define TI_AM3_DPLL_CORE_CLOCK			3
#define TI_AM3_DPLL_X2_CLOCK			2
#define TI_OMAP2_DPLL_CORE_CLOCK		1
#define TI_DPLL_END				0

static struct ofw_compat_data compat_data[] = {
	{ "ti,omap3-dpll-clock",	TI_OMAP3_DPLL_CLOCK },
	{ "ti,omap3-dpll-core-clock",	TI_OMAP3_DPLL_CORE_CLOCK },
	{ "ti,omap3-dpll-per-clock",	TI_OMAP3_DPLL_PER_CLOCK },
	{ "ti,omap3-dpll-per-j-type-clock",TI_OMAP3_DPLL_PER_J_TYPE_CLOCK },
	{ "ti,omap4-dpll-clock",	TI_OMAP4_DPLL_CLOCK },
	{ "ti,omap4-dpll-x2-clock",	TI_OMAP4_DPLL_X2_CLOCK },
	{ "ti,omap4-dpll-core-clock",	TI_OMAP4_DPLL_CORE_CLOCK },
	{ "ti,omap4-dpll-m4xen-clock",	TI_OMAP4_DPLL_M4XEN_CLOCK },
	{ "ti,omap4-dpll-j-type-clock",	TI_OMAP4_DPLL_J_TYPE_CLOCK },
	{ "ti,omap5-mpu-dpll-clock",	TI_OMAP5_MPU_DPLL_CLOCK },
	{ "ti,am3-dpll-no-gate-clock",	TI_AM3_DPLL_NO_GATE_CLOCK },
	{ "ti,am3-dpll-j-type-clock",	TI_AM3_DPLL_J_TYPE_CLOCK },
	{ "ti,am3-dpll-no-gate-j-type-clock",TI_AM3_DPLL_NO_GATE_J_TYPE_CLOCK },
	{ "ti,am3-dpll-clock",		TI_AM3_DPLL_CLOCK },
	{ "ti,am3-dpll-core-clock",	TI_AM3_DPLL_CORE_CLOCK },
	{ "ti,am3-dpll-x2-clock",	TI_AM3_DPLL_X2_CLOCK },
	{ "ti,omap2-dpll-core-clock",	TI_OMAP2_DPLL_CORE_CLOCK },
	{ NULL,				TI_DPLL_END }
};

static int
register_clk(struct ti_dpll_softc *sc) {
	int err;

	sc->clkdom = clkdom_create(sc->dev);
	if (sc->clkdom == NULL) {
		DPRINTF(sc->dev, "Failed to create clkdom\n");
		return (ENXIO);
	}

	err = ti_clknode_dpll_register(sc->clkdom, &sc->dpll_def);
	if (err) {
		DPRINTF(sc->dev,
			"ti_clknode_dpll_register failed %x\n", err);
		return (ENXIO);
	}

	err = clkdom_finit(sc->clkdom);
	if (err) {
		DPRINTF(sc->dev, "Clk domain finit fails %x.\n", err);
		return (ENXIO);
	}

	return (0);
}

static int
ti_dpll_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI DPLL Clock");

	return (BUS_PROBE_DEFAULT);
}

static int
parse_dpll_reg(struct ti_dpll_softc *sc) {
	ssize_t numbytes_regs;
	uint32_t num_regs;
	phandle_t node;
	cell_t reg_cells[4];

	if (sc->dpll_type == TI_AM3_DPLL_X2_CLOCK ||
		sc->dpll_type == TI_OMAP4_DPLL_X2_CLOCK) {
		sc->dpll_def.ti_clksel_mult.value = 2;
		sc->dpll_def.ti_clksel_mult.flags = TI_CLK_FACTOR_FIXED;

		sc->dpll_def.ti_clksel_div.value = 1;
		sc->dpll_def.ti_clksel_div.flags = TI_CLK_FACTOR_FIXED;
		return (0);
	}

	node = ofw_bus_get_node(sc->dev);

	numbytes_regs = OF_getproplen(node, "reg");
	num_regs = numbytes_regs / sizeof(cell_t);

	/* Sanity check */
	if (num_regs > 4)
		return (ENXIO);

	OF_getencprop(node, "reg", reg_cells, numbytes_regs);

	switch (sc->dpll_type) {
		case TI_AM3_DPLL_NO_GATE_CLOCK:
		case TI_AM3_DPLL_J_TYPE_CLOCK:
		case TI_AM3_DPLL_NO_GATE_J_TYPE_CLOCK:
		case TI_AM3_DPLL_CLOCK:
		case TI_AM3_DPLL_CORE_CLOCK:
		case TI_AM3_DPLL_X2_CLOCK:
			if (num_regs != 3)
				return (ENXIO);
			sc->dpll_def.ti_clkmode_offset = reg_cells[0];
			sc->dpll_def.ti_idlest_offset = reg_cells[1];
			sc->dpll_def.ti_clksel_offset = reg_cells[2];
			break;

		case TI_OMAP2_DPLL_CORE_CLOCK:
			if (num_regs != 2)
				return (ENXIO);
			sc->dpll_def.ti_clkmode_offset = reg_cells[0];
			sc->dpll_def.ti_clksel_offset = reg_cells[1];
			break;

		default:
			sc->dpll_def.ti_clkmode_offset = reg_cells[0];
			sc->dpll_def.ti_idlest_offset = reg_cells[1];
			sc->dpll_def.ti_clksel_offset = reg_cells[2];
			sc->dpll_def.ti_autoidle_offset = reg_cells[3];
			break;
	}

	/* AM335x */
	if (sc->dpll_def.ti_clksel_offset == CM_CLKSEL_DPLL_PERIPH) {
		sc->dpll_def.ti_clksel_mult.shift = 8;
		sc->dpll_def.ti_clksel_mult.mask = 0x000FFF00;
		sc->dpll_def.ti_clksel_mult.width = 12;
		sc->dpll_def.ti_clksel_mult.value = 0;
		sc->dpll_def.ti_clksel_mult.min_value = 2;
		sc->dpll_def.ti_clksel_mult.max_value = 4095;
		sc->dpll_def.ti_clksel_mult.flags = TI_CLK_FACTOR_ZERO_BASED |
						TI_CLK_FACTOR_MIN_VALUE |
						TI_CLK_FACTOR_MAX_VALUE;

		sc->dpll_def.ti_clksel_div.shift = 0;
		sc->dpll_def.ti_clksel_div.mask = 0x000000FF;
		sc->dpll_def.ti_clksel_div.width = 8;
		sc->dpll_def.ti_clksel_div.value = 0;
		sc->dpll_def.ti_clksel_div.min_value = 0;
		sc->dpll_def.ti_clksel_div.max_value = 255;
		sc->dpll_def.ti_clksel_div.flags = TI_CLK_FACTOR_MIN_VALUE |
						TI_CLK_FACTOR_MAX_VALUE;
	} else {
		sc->dpll_def.ti_clksel_mult.shift = 8;
		sc->dpll_def.ti_clksel_mult.mask = 0x0007FF00;
		sc->dpll_def.ti_clksel_mult.width = 11;
		sc->dpll_def.ti_clksel_mult.value = 0;
		sc->dpll_def.ti_clksel_mult.min_value = 2;
		sc->dpll_def.ti_clksel_mult.max_value = 2047;
		sc->dpll_def.ti_clksel_mult.flags = TI_CLK_FACTOR_ZERO_BASED |
						TI_CLK_FACTOR_MIN_VALUE |
						TI_CLK_FACTOR_MAX_VALUE;

		sc->dpll_def.ti_clksel_div.shift = 0;
		sc->dpll_def.ti_clksel_div.mask = 0x0000007F;
		sc->dpll_def.ti_clksel_div.width = 7;
		sc->dpll_def.ti_clksel_div.value = 0;
		sc->dpll_def.ti_clksel_div.min_value = 0;
		sc->dpll_def.ti_clksel_div.max_value = 127;
		sc->dpll_def.ti_clksel_div.flags = TI_CLK_FACTOR_MIN_VALUE |
						TI_CLK_FACTOR_MAX_VALUE;
	}
	DPRINTF(sc->dev, "clkmode %x idlest %x clksel %x autoidle %x\n",
		sc->dpll_def.ti_clkmode_offset, sc->dpll_def.ti_idlest_offset,
		sc->dpll_def.ti_clksel_offset,
		sc->dpll_def.ti_autoidle_offset);

	return (0);
}
static int
ti_dpll_attach(device_t dev)
{
	struct ti_dpll_softc *sc;
	phandle_t node;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->dpll_type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	node = ofw_bus_get_node(dev);

	/* Grab the content of reg properties */
	parse_dpll_reg(sc);

	/* default flags (OMAP4&AM335x) not present in the dts at moment */
	sc->dpll_def.ti_clkmode_flags = MN_BYPASS_MODE_FLAG | LOCK_MODE_FLAG;

	if (OF_hasprop(node, "ti,low-power-stop")) {
		sc->dpll_def.ti_clkmode_flags |= LOW_POWER_STOP_MODE_FLAG;
	}
	if (OF_hasprop(node, "ti,low-power-bypass")) {
		sc->dpll_def.ti_clkmode_flags |= IDLE_BYPASS_LOW_POWER_MODE_FLAG;
	}
	if (OF_hasprop(node, "ti,lock")) {
		sc->dpll_def.ti_clkmode_flags |= LOCK_MODE_FLAG;
	}

	read_clock_cells(sc->dev, &sc->clock_cell);

	create_clkdef(sc->dev, &sc->clock_cell, &sc->dpll_def.clkdef);

	err = find_parent_clock_names(sc->dev, &sc->clock_cell,
			&sc->dpll_def.clkdef);

	if (err) {
		/* free_clkdef will be called in ti_dpll_new_pass */
		DPRINTF(sc->dev, "find_parent_clock_names failed\n");
		return (bus_generic_attach(sc->dev));
	}

	err = register_clk(sc);

	if (err) {
		/* free_clkdef will be called in ti_dpll_new_pass */
		DPRINTF(sc->dev, "register_clk failed\n");
		return (bus_generic_attach(sc->dev));
	}

	sc->attach_done = true;

	free_clkdef(&sc->dpll_def.clkdef);

	return (bus_generic_attach(sc->dev));
}

static int
ti_dpll_detach(device_t dev)
{
	return (EBUSY);
}

static void
ti_dpll_new_pass(device_t dev)
{
	struct ti_dpll_softc *sc;
	int err;

	sc = device_get_softc(dev);

	if (sc->attach_done) {
		return;
	}

	err = find_parent_clock_names(sc->dev, &sc->clock_cell,
		&sc->dpll_def.clkdef);
	if (err) {
		/* free_clkdef will be called in a later call to ti_dpll_new_pass */
		DPRINTF(sc->dev,
			"new_pass find_parent_clock_names failed\n");
		return;
	}

	err = register_clk(sc);
	if (err) {
		/* free_clkdef will be called in a later call to ti_dpll_new_pass */
		DPRINTF(sc->dev, "new_pass register_clk failed\n");
		return;
	}

	sc->attach_done = true;
	free_clkdef(&sc->dpll_def.clkdef);
}

static device_method_t ti_dpll_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_dpll_probe),
	DEVMETHOD(device_attach,	ti_dpll_attach),
	DEVMETHOD(device_detach,	ti_dpll_detach),

	/* Bus interface */
	DEVMETHOD(bus_new_pass,		ti_dpll_new_pass),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ti_dpll, ti_dpll_driver, ti_dpll_methods,
	sizeof(struct ti_dpll_softc));

EARLY_DRIVER_MODULE(ti_dpll, simplebus, ti_dpll_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ti_dpll, 1);
