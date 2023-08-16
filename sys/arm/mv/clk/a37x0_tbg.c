/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Semihalf.
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clkdev_if.h"

#include "a37x0_tbg_pll.h"

#define NUM_TBG			4

#define TBG_CTRL0		0x4
#define TBG_CTRL1		0x8
#define TBG_CTRL7		0x20
#define TBG_CTRL8		0x30

#define TBG_MASK		0x1FF

#define TBG_A_REFDIV		0
#define TBG_B_REFDIV		16

#define TBG_A_FBDIV		2
#define TBG_B_FBDIV		18

#define TBG_A_VCODIV_SEL	0
#define TBG_B_VCODIV_SEL	16

#define TBG_A_VCODIV_DIFF	1
#define TBG_B_VCODIV_DIFF	17

struct a37x0_tbg_softc {
	device_t 		dev;
	struct clkdom		*clkdom;
	struct resource		*res;
};

static struct resource_spec a37x0_tbg_clk_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct a37x0_tbg_def {
	char 			*name;
	uint32_t		refdiv_shift;
	uint32_t		fbdiv_shift;
	uint32_t		vcodiv_offset;
	uint32_t		vcodiv_shift;
	uint32_t		tbg_bypass_en;
};

static const struct a37x0_tbg_def tbg[NUM_TBG] = {
	{"TBG-A-P", TBG_A_REFDIV, TBG_A_FBDIV, TBG_CTRL8, TBG_A_VCODIV_DIFF, 9},
	{"TBG-B-P", TBG_B_REFDIV, TBG_B_FBDIV, TBG_CTRL8,
	    TBG_B_VCODIV_DIFF, 25},
	{"TBG-A-S", TBG_A_REFDIV, TBG_A_FBDIV, TBG_CTRL1, TBG_A_VCODIV_SEL, 9},
	{"TBG-B-S", TBG_B_REFDIV, TBG_B_FBDIV, TBG_CTRL1, TBG_B_VCODIV_SEL, 25}
};

static int a37x0_tbg_read_4(device_t, bus_addr_t, uint32_t *);
static int a37x0_tbg_attach(device_t);
static int a37x0_tbg_detach(device_t);
static int a37x0_tbg_probe(device_t);

static device_method_t a37x0_tbg_methods [] = {
	DEVMETHOD(device_attach,	a37x0_tbg_attach),
	DEVMETHOD(device_detach,	a37x0_tbg_detach),
	DEVMETHOD(device_probe,		a37x0_tbg_probe),

	DEVMETHOD(clkdev_read_4,	a37x0_tbg_read_4),

	DEVMETHOD_END
};

static driver_t a37x0_tbg_driver = {
	"a37x0_tbg",
	a37x0_tbg_methods,
	sizeof(struct a37x0_tbg_softc)
};

EARLY_DRIVER_MODULE(a37x0_tbg, simplebus, a37x0_tbg_driver, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE);

static int
a37x0_tbg_read_4(device_t dev, bus_addr_t offset, uint32_t *val)
{
	struct a37x0_tbg_softc *sc;

	sc = device_get_softc(dev);

	*val = bus_read_4(sc->res, offset);

	return (0);
}

static int
a37x0_tbg_attach(device_t dev)
{
	struct a37x0_tbg_pll_clk_def def;
	struct a37x0_tbg_softc *sc;
	const char *clkname;
	int error, i;
	phandle_t node;
	clk_t clock;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, a37x0_tbg_clk_spec, &sc->res) != 0) {
		device_printf(dev, "Cannot allocate resources\n");
		return (ENXIO);
	}

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "Cannot create clock domain.\n");
		return (ENXIO);
	}

	error = clk_get_by_ofw_index(dev, node, 0, &clock);
	if (error != 0) {
		device_printf(dev, "Cannot find clock parent\n");
		bus_release_resources(dev, a37x0_tbg_clk_spec, &sc->res);
		return (error);
	}

	clkname = clk_get_name(clock);

	for (i = 0; i < NUM_TBG; i++) {
		def.clkdef.parent_names = &clkname;
		def.clkdef.parent_cnt = 1;
		def.clkdef.id = i;
		def.clkdef.name = tbg[i].name;

		def.vcodiv.offset = tbg[i].vcodiv_offset;
		def.vcodiv.shift = tbg[i].vcodiv_shift;
		def.refdiv.offset = TBG_CTRL7;
		def.refdiv.shift = tbg[i].refdiv_shift;
		def.fbdiv.offset = TBG_CTRL0;
		def.fbdiv.shift = tbg[i].fbdiv_shift;
		def.vcodiv.mask = def.refdiv.mask = def.fbdiv.mask = TBG_MASK;
		def.tbg_bypass.offset = TBG_CTRL1;
		def.tbg_bypass.shift = tbg[i].tbg_bypass_en;
		def.tbg_bypass.mask = 0x1;

		error = a37x0_tbg_pll_clk_register(sc->clkdom, &def);

		if (error) {
			device_printf(dev, "Cannot register clock node\n");
			bus_release_resources(dev, a37x0_tbg_clk_spec,
			    &sc->res);
			return (ENXIO);
		}
	}

	error = clkdom_finit(sc->clkdom);
	if (error) {
		device_printf(dev,
		    "Cannot finalize clock domain intialization\n");
		bus_release_resources(dev, a37x0_tbg_clk_spec, &sc->res);
		return (ENXIO);
	}

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);
}

static int
a37x0_tbg_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "marvell,armada-3700-tbg-clock"))
		return (ENXIO);

	device_set_desc(dev, "Marvell Armada 3700 time base generators");
	return (BUS_PROBE_DEFAULT);
}

static int
a37x0_tbg_detach(device_t dev)
{

	return (EBUSY);
}
