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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clkdev_if.h"
#include "periph.h"

#define TBG_COUNT 4
#define XTAL_OFW_INDEX 4

static struct resource_spec a37x0_periph_clk_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

int
a37x0_periph_clk_attach(device_t dev)
{
	struct a37x0_periph_clknode_def *dev_defs;
	struct a37x0_periph_clk_softc *sc;
	const char *tbg_clocks[5];
	const char *xtal_clock;
	phandle_t node;
	int error, i;
	clk_t clock;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, a37x0_periph_clk_spec, &sc->res) != 0) {
		device_printf(dev, "Cannot allocate resources\n");
		return (ENXIO);
	}

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "Cannot create clock domain\n");
		return (ENXIO);
	}

	for (i = 0; i < TBG_COUNT; i++){
		error = clk_get_by_ofw_index(dev, node, i, &clock);
		if (error)
			goto fail;
		tbg_clocks[i] = clk_get_name(clock);
	}

	error = clk_get_by_ofw_index(dev, node, XTAL_OFW_INDEX, &clock);
	if (error)
		goto fail;
	xtal_clock = clk_get_name(clock);

	dev_defs = sc->devices;

	for (i = 0; i< sc->device_count; i++) {
		dev_defs[i].common_def.tbgs = tbg_clocks;
		dev_defs[i].common_def.xtal = xtal_clock;
		dev_defs[i].common_def.tbg_cnt = TBG_COUNT;
		switch (dev_defs[i].type) {
		case CLK_FULL_DD:
			error = a37x0_periph_d_register_full_clk_dd(
			    sc->clkdom, &dev_defs[i]);
			if (error)
				goto fail;
			break;

		case CLK_FULL:
			error = a37x0_periph_d_register_full_clk(
			    sc->clkdom, &dev_defs[i]);
			if (error)
				goto fail;
			break;

		case CLK_GATE:
			error = a37x0_periph_gate_register_gate(
			    sc->clkdom, &dev_defs[i]);
			if (error)
				goto fail;
			break;

		case CLK_MUX_GATE:
			error = a37x0_periph_register_mux_gate(
			   sc->clkdom, &dev_defs[i]);
			if (error)
				goto fail;
			break;

		case CLK_FIXED:
			error = a37x0_periph_fixed_register_fixed(
			   sc->clkdom, &dev_defs[i]);
			if (error)
				goto fail;
			break;

		case CLK_CPU:
			error = a37x0_periph_d_register_periph_cpu(
			   sc->clkdom, &dev_defs[i]);
			if (error)
				goto fail;
			break;

		case CLK_MDD:
			error = a37x0_periph_d_register_mdd(
			    sc->clkdom, &dev_defs[i]);
			if (error)
				goto fail;
			break;

		case CLK_MUX_GATE_FIXED:
			error = a37x0_periph_register_mux_gate_fixed(
			    sc->clkdom, &dev_defs[i]);
			if (error)
				goto fail;
			break;

		default:
			return (ENXIO);
		}
	}

	error = clkdom_finit(sc->clkdom);
	if (error)
		goto fail;

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);

fail:
	bus_release_resources(dev, a37x0_periph_clk_spec, &sc->res);

	return (error);

}

int
a37x0_periph_clk_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct a37x0_periph_clk_softc *sc;

	sc = device_get_softc(dev);
	*val = bus_read_4(sc->res, addr);

	return (0);
}

void
a37x0_periph_clk_device_lock(device_t dev)
{
	struct a37x0_periph_clk_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

void
a37x0_periph_clk_device_unlock(device_t dev)
{
	struct a37x0_periph_clk_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

int
a37x0_periph_clk_detach(device_t dev)
{

	return (EBUSY);
}
