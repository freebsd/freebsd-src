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

#include <dev/clk/clk_gate.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clock_common.h"

#define DEBUG_GATE	0

#if DEBUG_GATE
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

/*
 * Devicetree description
 * Documentation/devicetree/bindings/clock/ti/gate.txt
 */

struct ti_gate_softc {
	device_t		sc_dev;
	bool			attach_done;
	uint8_t			sc_type;

	struct clk_gate_def	gate_def;
	struct clock_cell_info  clock_cell;
	struct clkdom		*clkdom;
};

static int ti_gate_probe(device_t dev);
static int ti_gate_attach(device_t dev);
static int ti_gate_detach(device_t dev);

#define TI_GATE_CLOCK			7
#define TI_WAIT_GATE_CLOCK		6
#define TI_DSS_GATE_CLOCK		5
#define TI_AM35XX_GATE_CLOCK		4
#define TI_CLKDM_GATE_CLOCK		3
#define TI_HSDIV_GATE_CLOCK		2
#define TI_COMPOSITE_NO_WAIT_GATE_CLOCK	1
#define TI_GATE_END			0

static struct ofw_compat_data compat_data[] = {
	{ "ti,gate-clock",			TI_GATE_CLOCK },
	{ "ti,wait-gate-clock",			TI_WAIT_GATE_CLOCK },
	{ "ti,dss-gate-clock",			TI_DSS_GATE_CLOCK },
	{ "ti,am35xx-gate-clock",		TI_AM35XX_GATE_CLOCK },
	{ "ti,clkdm-gate-clock",		TI_CLKDM_GATE_CLOCK },
	{ "ti,hsdiv-gate-cloc",			TI_HSDIV_GATE_CLOCK },
	{ "ti,composite-no-wait-gate-clock",	TI_COMPOSITE_NO_WAIT_GATE_CLOCK },
	{ NULL,					TI_GATE_END }
};

static int
ti_gate_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI Gate Clock");

	return (BUS_PROBE_DEFAULT);
}

static int
register_clk(struct ti_gate_softc *sc) {
	int err;
	sc->clkdom = clkdom_create(sc->sc_dev);
	if (sc->clkdom == NULL) {
		DPRINTF(sc->sc_dev, "Failed to create clkdom\n");
		return ENXIO;
	}

	err = clknode_gate_register(sc->clkdom, &sc->gate_def);
	if (err) {
		DPRINTF(sc->sc_dev, "clknode_gate_register failed %x\n", err);
		return ENXIO;
	}

	err = clkdom_finit(sc->clkdom);
	if (err) {
		DPRINTF(sc->sc_dev, "Clk domain finit fails %x.\n", err);
		return ENXIO;
	}

	return (0);
}

static int
ti_gate_attach(device_t dev)
{
	struct ti_gate_softc *sc;
	phandle_t node;
	int err;
	cell_t value;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	node = ofw_bus_get_node(dev);

	/* Get the compatible type */
	sc->sc_type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	/* Get the content of reg properties */
	if (sc->sc_type != TI_CLKDM_GATE_CLOCK) {
		OF_getencprop(node, "reg", &value, sizeof(value));
		sc->gate_def.offset = value;
	}
#if DEBUG_GATE
	else {
		DPRINTF(sc->sc_dev, "no reg (TI_CLKDM_GATE_CLOCK)\n");
	}
#endif

	if (OF_hasprop(node, "ti,bit-shift")) {
		OF_getencprop(node, "ti,bit-shift", &value, sizeof(value));
		sc->gate_def.shift = value;
		DPRINTF(sc->sc_dev, "ti,bit-shift => shift %x\n", sc->gate_def.shift);
	}
	if (OF_hasprop(node, "ti,set-bit-to-disable")) {
		sc->gate_def.on_value = 0;
		sc->gate_def.off_value = 1;
		DPRINTF(sc->sc_dev,
			"on_value = 0, off_value = 1 (ti,set-bit-to-disable)\n");
	} else {
		sc->gate_def.on_value = 1;
		sc->gate_def.off_value = 0;
		DPRINTF(sc->sc_dev, "on_value = 1, off_value = 0\n");
	}

	sc->gate_def.gate_flags = 0x0;

	read_clock_cells(sc->sc_dev, &sc->clock_cell);

	create_clkdef(sc->sc_dev, &sc->clock_cell, &sc->gate_def.clkdef);

	/* Calculate mask */
	sc->gate_def.mask = (1 << fls(sc->clock_cell.num_real_clocks)) - 1;
	DPRINTF(sc->sc_dev, "num_real_clocks %x gate_def.mask %x\n",
		sc->clock_cell.num_real_clocks, sc->gate_def.mask);

	err = find_parent_clock_names(sc->sc_dev, &sc->clock_cell, &sc->gate_def.clkdef);

	if (err) {
		/* free_clkdef will be called in ti_gate_new_pass */
		DPRINTF(sc->sc_dev, "find_parent_clock_names failed\n");
		return (bus_generic_attach(sc->sc_dev));
	}

	err = register_clk(sc);

	if (err) {
		/* free_clkdef will be called in ti_gate_new_pass */
		DPRINTF(sc->sc_dev, "register_clk failed\n");
		return (bus_generic_attach(sc->sc_dev));
	}

	sc->attach_done = true;

	free_clkdef(&sc->gate_def.clkdef);

	return (bus_generic_attach(sc->sc_dev));
}

static int
ti_gate_detach(device_t dev)
{
	return (EBUSY);
}

static void
ti_gate_new_pass(device_t dev) {
	struct ti_gate_softc *sc;
	int err;

	sc = device_get_softc(dev);

	if (sc->attach_done) {
		return;
	}

	err = find_parent_clock_names(sc->sc_dev, &sc->clock_cell, &sc->gate_def.clkdef);
	if (err) {
		/* free_clkdef will be called in later call to ti_gate_new_pass */
		DPRINTF(sc->sc_dev, "new_pass find_parent_clock_names failed\n");
		return;
	}

	err = register_clk(sc);
	if (err) {
		/* free_clkdef will be called in later call to ti_gate_new_pass */
		DPRINTF(sc->sc_dev, "new_pass register_clk failed\n");
		return;
	}

	sc->attach_done = true;

	free_clkdef(&sc->gate_def.clkdef);
}

static device_method_t ti_gate_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_gate_probe),
	DEVMETHOD(device_attach,	ti_gate_attach),
	DEVMETHOD(device_detach,	ti_gate_detach),

	/* Bus interface */
	DEVMETHOD(bus_new_pass,		ti_gate_new_pass),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ti_gate, ti_gate_driver, ti_gate_methods,
	sizeof(struct ti_gate_softc));

EARLY_DRIVER_MODULE(ti_gate, simplebus, ti_gate_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ti_gate, 1);
