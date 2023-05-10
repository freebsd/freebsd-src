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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/libkern.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <dev/fdt/simplebus.h>

#include <dev/extres/clk/clk_mux.h>
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
 * Documentation/devicetree/bindings/clock/ti/mux.txt
 */

struct ti_mux_softc {
	device_t		sc_dev;
	bool			attach_done;

	struct clk_mux_def	mux_def;
	struct clock_cell_info	clock_cell;
	struct clkdom 		*clkdom;
};

static int ti_mux_probe(device_t dev);
static int ti_mux_attach(device_t dev);
static int ti_mux_detach(device_t dev);

#define TI_MUX_CLOCK			2
#define TI_COMPOSITE_MUX_CLOCK		1
#define TI_MUX_END			0

static struct ofw_compat_data compat_data[] = {
	{ "ti,mux-clock",		TI_MUX_CLOCK },
	{ "ti,composite-mux-clock",	TI_COMPOSITE_MUX_CLOCK },
	{ NULL,				TI_MUX_END }
};

static int
ti_mux_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI Mux Clock");

	return (BUS_PROBE_DEFAULT);
}

static int
register_clk(struct ti_mux_softc *sc) {
	int err;

	sc->clkdom = clkdom_create(sc->sc_dev);
	if (sc->clkdom == NULL) {
		DPRINTF(sc->sc_dev, "Failed to create clkdom\n");
		return ENXIO;
	}

	err = clknode_mux_register(sc->clkdom, &sc->mux_def);
	if (err) {
		DPRINTF(sc->sc_dev, "clknode_mux_register failed %x\n", err);
		return ENXIO;
	}

	err = clkdom_finit(sc->clkdom);
	if (err) {
		DPRINTF(sc->sc_dev, "Clk domain finit fails %x.\n", err);
		return ENXIO;
	}

	return 0;
}

static int
ti_mux_attach(device_t dev)
{
	struct ti_mux_softc *sc;
	phandle_t node;
	int err;
	cell_t value;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	node = ofw_bus_get_node(dev);

	/* Grab the content of reg properties */
	OF_getencprop(node, "reg", &value, sizeof(value));
	sc->mux_def.offset = value;

	if (OF_hasprop(node, "ti,bit-shift")) {
		OF_getencprop(node, "ti,bit-shift", &value, sizeof(value));
		sc->mux_def.shift = value;
		DPRINTF(sc->sc_dev, "ti,bit-shift => shift %x\n", sc->mux_def.shift);
	}
	if (OF_hasprop(node, "ti,index-starts-at-one")) {
		/* FIXME: Add support in dev/extres/clk */
		/*sc->mux_def.mux_flags =  ... */
		device_printf(sc->sc_dev, "ti,index-starts-at-one - Not implemented\n");
	}

	if (OF_hasprop(node, "ti,set-rate-parent"))
		device_printf(sc->sc_dev, "ti,set-rate-parent - Not implemented\n");
	if (OF_hasprop(node, "ti,latch-bit"))
		device_printf(sc->sc_dev, "ti,latch-bit - Not implemented\n");

	read_clock_cells(sc->sc_dev, &sc->clock_cell);

	create_clkdef(sc->sc_dev, &sc->clock_cell, &sc->mux_def.clkdef);

	/* Figure out the width from ti_max_div */
	if (sc->mux_def.mux_flags)
		sc->mux_def.width = fls(sc->clock_cell.num_real_clocks-1);
	else
		sc->mux_def.width = fls(sc->clock_cell.num_real_clocks);

	DPRINTF(sc->sc_dev, "sc->clock_cell.num_real_clocks %x def.width %x\n",
		sc->clock_cell.num_real_clocks, sc->mux_def.width);

	err = find_parent_clock_names(sc->sc_dev, &sc->clock_cell, &sc->mux_def.clkdef);

	if (err) {
		/* free_clkdef will be called in ti_mux_new_pass */
		DPRINTF(sc->sc_dev, "find_parent_clock_names failed\n");
		return (bus_generic_attach(sc->sc_dev));
	}

	err = register_clk(sc);

	if (err) {
		/* free_clkdef will be called in ti_mux_new_pass */
		DPRINTF(sc->sc_dev, "register_clk failed\n");
		return (bus_generic_attach(sc->sc_dev));
	}

	sc->attach_done = true;

	free_clkdef(&sc->mux_def.clkdef);

	return (bus_generic_attach(sc->sc_dev));
}

static void
ti_mux_new_pass(device_t dev)
{
	struct ti_mux_softc *sc;
	int err;

	sc = device_get_softc(dev);

	if (sc->attach_done) {
		return;
	}

	err = find_parent_clock_names(sc->sc_dev, &sc->clock_cell, &sc->mux_def.clkdef);
	if (err) {
		/* free_clkdef will be called in later call to ti_mux_new_pass */
		DPRINTF(sc->sc_dev, "ti_mux_new_pass find_parent_clock_names failed\n");
		return;
	}

	err = register_clk(sc);
	if (err) {
		/* free_clkdef will be called in later call to ti_mux_new_pass */
		DPRINTF(sc->sc_dev, "ti_mux_new_pass register_clk failed\n");
		return;
	}

	sc->attach_done = true;

	free_clkdef(&sc->mux_def.clkdef);
}

static int
ti_mux_detach(device_t dev)
{
	return (EBUSY);
}

static device_method_t ti_mux_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_mux_probe),
	DEVMETHOD(device_attach,	ti_mux_attach),
	DEVMETHOD(device_detach,	ti_mux_detach),

	/* Bus interface */
	DEVMETHOD(bus_new_pass,		ti_mux_new_pass),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ti_mux, ti_mux_driver, ti_mux_methods,
	sizeof(struct ti_mux_softc));

EARLY_DRIVER_MODULE(ti_mux, simplebus, ti_mux_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ti_mux, 1);
