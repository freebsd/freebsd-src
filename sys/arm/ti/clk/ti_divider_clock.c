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

#include "clock_common.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

/*
 * Devicetree description
 * Documentation/devicetree/bindings/clock/ti/divider.txt
 */

struct ti_divider_softc {
	device_t		sc_dev;
	bool			attach_done;
	struct clk_div_def	div_def;

	struct clock_cell_info	clock_cell;
	struct clkdom		*clkdom;
};

static int ti_divider_probe(device_t dev);
static int ti_divider_attach(device_t dev);
static int ti_divider_detach(device_t dev);

#define TI_DIVIDER_CLOCK		2
#define TI_COMPOSITE_DIVIDER_CLOCK	1
#define TI_DIVIDER_END			0

static struct ofw_compat_data compat_data[] = {
	{ "ti,divider-clock",		TI_DIVIDER_CLOCK },
	{ "ti,composite-divider-clock",	TI_COMPOSITE_DIVIDER_CLOCK },
	{ NULL,				TI_DIVIDER_END }
};

static int
register_clk(struct ti_divider_softc *sc) {
	int err;

	sc->clkdom = clkdom_create(sc->sc_dev);
	if (sc->clkdom == NULL) {
		DPRINTF(sc->sc_dev, "Failed to create clkdom\n");
		return (ENXIO);
	}

	err = clknode_div_register(sc->clkdom, &sc->div_def);
	if (err) {
		DPRINTF(sc->sc_dev, "clknode_div_register failed %x\n", err);
		return (ENXIO);
	}

	err = clkdom_finit(sc->clkdom);
	if (err) {
		DPRINTF(sc->sc_dev, "Clk domain finit fails %x.\n", err);
		return (ENXIO);
	}

	return (0);
}

static int
ti_divider_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI Divider Clock");

	return (BUS_PROBE_DEFAULT);
}

static int
ti_divider_attach(device_t dev)
{
	struct ti_divider_softc *sc;
	phandle_t	node;
	int		err;
	cell_t		value;
	uint32_t	ti_max_div;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	node = ofw_bus_get_node(dev);

	/* Grab the content of reg properties */
	OF_getencprop(node, "reg", &value, sizeof(value));
	sc->div_def.offset = value;

	if (OF_hasprop(node, "ti,bit-shift")) {
		OF_getencprop(node, "ti,bit-shift", &value, sizeof(value));
		sc->div_def.i_shift = value;
	}

	if (OF_hasprop(node, "ti,index-starts-at-one")) {
		sc->div_def.div_flags = CLK_DIV_ZERO_BASED;
	}

	if (OF_hasprop(node, "ti,index-power-of-two")) {
		/* FIXME: later */
		device_printf(sc->sc_dev, "ti,index-power-of-two - Not implemented\n");
		/* remember to update i_width a few lines below */
	}
	if (OF_hasprop(node, "ti,max-div")) {
		OF_getencprop(node, "ti,max-div", &value, sizeof(value));
		ti_max_div = value;
	}

	if (OF_hasprop(node, "clock-output-names"))
		device_printf(sc->sc_dev, "clock-output-names\n");
	if (OF_hasprop(node, "ti,dividers"))
		device_printf(sc->sc_dev, "ti,dividers\n");
	if (OF_hasprop(node, "ti,min-div"))
		device_printf(sc->sc_dev, "ti,min-div - Not implemented\n");

	if (OF_hasprop(node, "ti,autoidle-shift"))
		device_printf(sc->sc_dev, "ti,autoidle-shift - Not implemented\n");
	if (OF_hasprop(node, "ti,set-rate-parent"))
		device_printf(sc->sc_dev, "ti,set-rate-parent - Not implemented\n");
	if (OF_hasprop(node, "ti,latch-bit"))
		device_printf(sc->sc_dev, "ti,latch-bit - Not implemented\n");

	/* Figure out the width from ti_max_div */
	if (sc->div_def.div_flags)
		sc->div_def.i_width = fls(ti_max_div-1);
	else
		sc->div_def.i_width = fls(ti_max_div);

	DPRINTF(sc->sc_dev, "div_def.i_width %x\n", sc->div_def.i_width);

	read_clock_cells(sc->sc_dev, &sc->clock_cell);

	create_clkdef(sc->sc_dev, &sc->clock_cell, &sc->div_def.clkdef);

	err = find_parent_clock_names(sc->sc_dev, &sc->clock_cell, &sc->div_def.clkdef);

	if (err) {
		/* free_clkdef will be called in ti_divider_new_pass */
		DPRINTF(sc->sc_dev, "find_parent_clock_names failed\n");
		return (bus_generic_attach(sc->sc_dev));
	}

	err = register_clk(sc);

	if (err) {
		/* free_clkdef will be called in ti_divider_new_pass */
		DPRINTF(sc->sc_dev, "register_clk failed\n");
		return (bus_generic_attach(sc->sc_dev));
	}

	sc->attach_done = true;

	free_clkdef(&sc->div_def.clkdef);

	return (bus_generic_attach(sc->sc_dev));
}

static int
ti_divider_detach(device_t dev)
{
	return (EBUSY);
}

static void
ti_divider_new_pass(device_t dev)
{
	struct ti_divider_softc *sc;
	int err;

	sc = device_get_softc(dev);

	if (sc->attach_done) {
		return;
	}

	err = find_parent_clock_names(sc->sc_dev, &sc->clock_cell, &sc->div_def.clkdef);
	if (err) {
		/* free_clkdef will be called in a later call to ti_divider_new_pass */
		DPRINTF(sc->sc_dev, "new_pass find_parent_clock_names failed\n");
		return;
	}

	err = register_clk(sc);
	if (err) {
		/* free_clkdef will be called in a later call to ti_divider_new_pass */
		DPRINTF(sc->sc_dev, "new_pass register_clk failed\n");
		return;
	}

	sc->attach_done = true;

	free_clkdef(&sc->div_def.clkdef);
}

static device_method_t ti_divider_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_divider_probe),
	DEVMETHOD(device_attach,	ti_divider_attach),
	DEVMETHOD(device_detach,	ti_divider_detach),

	/* Bus interface */
	DEVMETHOD(bus_new_pass,		ti_divider_new_pass),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ti_divider, ti_divider_driver, ti_divider_methods,
	sizeof(struct ti_divider_softc));

EARLY_DRIVER_MODULE(ti_divider, simplebus, ti_divider_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ti_divider, 1);
