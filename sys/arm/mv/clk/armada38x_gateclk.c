/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Semihalf.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_gate.h>

#include "clkdev_if.h"

struct armada38x_gateclk_softc
{
	struct clkdom   *clkdom;
	struct mtx  mtx;
	const char* parent;
};

static struct clk_gate_def gateclk_nodes[] =
{
	{
		.clkdef = {
			.name = "gateclk-audio",
			.id = 0,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 0,
	},
	{
		.clkdef = {
			.name = "gateclk-eth2",
			.id = 2,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 2,
	},
	{
		.clkdef = {
			.name = "gateclk-eth1",
			.id = 3,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 3,
	},
	{
		.clkdef = {
			.name = "gateclk-eth0",
			.id = 4,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 4,
	},
	{
		.clkdef = {
			.name = "gateclk-mdio",
			.id = 4,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 4,
	},
	{
		.clkdef = {
			.name = "gateclk-usb3h0",
			.id = 9,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 9,
	},
	{
		.clkdef = {
			.name = "gateclk-usb3h1",
			.id = 10,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 10,
	},
	{
		.clkdef = {
			.name = "gateclk-bm",
			.id = 13,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 13,
	},
	{
		.clkdef = {
			.name = "gateclk-crypto0z",
			.id = 14,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 14,
	},
	{
		.clkdef = {
			.name = "gateclk-sata0",
			.id = 15,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 15,
	},
	{
		.clkdef = {
			.name = "gateclk-crypto1z",
			.id = 16,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 16,
	},
	{
		.clkdef = {
			.name = "gateclk-sdio",
			.id = 17,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 17,
	},
	{
		.clkdef = {
			.name = "gateclk-usb2",
			.id = 18,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 18,
	},
	{
		.clkdef = {
			.name = "gateclk-crypto1",
			.id = 21,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 21,
	},
	{
		.clkdef = {
			.name = "gateclk-xor0",
			.id = 22,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 22,
	},
	{
		.clkdef = {
			.name = "gateclk-crypto0",
			.id = 23,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 23,
	},
	{
		.clkdef = {
			.name = "gateclk-xor1",
			.id = 28,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 28,
	},
	{
		.clkdef = {
			.name = "gateclk-sata1",
			.id = 30,
			.parent_cnt = 1,
			.flags = 0,
		},
		.shift = 30,
	}
};

static int armada38x_gateclk_probe(device_t dev);
static int armada38x_gateclk_attach(device_t dev);

static device_method_t armada38x_gateclk_methods[] = {
	DEVMETHOD(device_probe,		armada38x_gateclk_probe),
	DEVMETHOD(device_attach,	armada38x_gateclk_attach),

	DEVMETHOD_END
};

static driver_t armada38x_gateclk_driver = {
	"armada38x_gateclk",
	armada38x_gateclk_methods,
	sizeof(struct armada38x_gateclk_softc),
};

EARLY_DRIVER_MODULE(armada38x_gateclk, simplebus, armada38x_gateclk_driver, 0, 0,
	BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 1);

static int
armada38x_gateclk_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if(!ofw_bus_is_compatible(dev, "marvell,armada-380-gating-clock"))
		return (ENXIO);

	device_set_desc(dev, "ARMADA38X gateclk");

	return (BUS_PROBE_DEFAULT);
}

static int
armada38x_gateclk_attach(device_t dev)
{
	struct armada38x_gateclk_softc *sc;
	phandle_t node;
	int i, error;
	clk_t clock;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "Cannot create clock domain.\n");
		return (ENXIO);
	}

	error = clk_get_by_ofw_index(dev, node, 0, &clock);
	if (error > 0)
		return (error);

	sc->parent = clk_get_name(clock);

	for (i = 0; i < nitems(gateclk_nodes); ++i) {
		gateclk_nodes[i].clkdef.parent_names = &sc->parent;
		error = clknode_gate_register(sc->clkdom, &gateclk_nodes[i]);
		if (error != 0) {
			device_printf(dev, "Cannot create gate nodes\n");
			return (error);
		}
	}

	if (clkdom_finit(sc->clkdom) != 0)
		panic("Cannot finalize clock domain initialization.\n");

	return (0);
}
