/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include <sys/cdefs.h>
__FBSDID("$FressBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/extres/clk/clk.h>

#include <arm/mv/mvwin.h>
#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#include <arm/mv/clk/armada38x_gen.h>

#include "clkdev_if.h"

#define ARMADA38X_CORECLK_MAXREG 0

static struct resource_spec armada38x_coreclk_specs[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct armada38x_coreclk_softc {
	struct resource	*res;
	struct clkdom	*clkdom;
	struct mtx	mtx;
};

static int armada38x_coreclk_attach(device_t dev);
static int armada38x_coreclk_probe(device_t dev);

static struct armada38x_gen_clknode_def gen_nodes[] =
{
	{
		.def = {
			.name = "coreclk_0",
			.id = 0,
			.parent_cnt = 0,
		},
	},
	{
		.def = {
			.name = "coreclk_2",
			.id = 1,
			.parent_cnt = 0,
		},
	}
};

static int
armada38x_coreclk_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct armada38x_coreclk_softc *sc;

	sc = device_get_softc(dev);

	if (addr > ARMADA38X_CORECLK_MAXREG)
		return (EINVAL);

	*val = bus_read_4(sc->res, addr);

	return (0);
}

static int
armada38x_coreclk_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct armada38x_coreclk_softc *sc;

	sc = device_get_softc(dev);

	if (addr > ARMADA38X_CORECLK_MAXREG)
		return (EINVAL);

	bus_write_4(sc->res, addr, val);

	return (0);
}

static void
armada38x_coreclk_device_lock(device_t dev)
{
	struct armada38x_coreclk_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
armada38x_coreclk_device_unlock(device_t dev)
{
	struct armada38x_coreclk_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static int
armada38x_coreclk_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "marvell,armada-380-core-clock"))
		return (ENXIO);

	device_set_desc(dev, "ARMADA38X core-clock");

	return (BUS_PROBE_DEFAULT);
}

static int
armada38x_coreclk_create_coreclk(device_t dev)
{
	struct armada38x_coreclk_softc *sc;
	int rv, i;

	sc = device_get_softc(dev);

	for (i = 0; i < nitems(gen_nodes); ++i) {
		rv = armada38x_gen_register(sc->clkdom, &gen_nodes[i]);
		if (rv)
			return (rv);
	}

	return (rv);
}

static int
armada38x_coreclk_attach(device_t dev)
{
	struct armada38x_coreclk_softc *sc;
	int error;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, armada38x_coreclk_specs, &sc->res) != 0) {
		device_printf(dev, "Cannot allocate resources.\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->clkdom = clkdom_create(dev);
	if (NULL == sc->clkdom) {
		device_printf(dev, "Cannot create clkdom\n");
		return (ENXIO);
	}

	error = armada38x_coreclk_create_coreclk(dev);
	if (0 != error) {
		device_printf(dev, "Cannot create coreclk.\n");
		return (error);
	}

	if (clkdom_finit(sc->clkdom) != 0)
		panic("Cannot finalize clock domain initialization.\n");

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);
}

static device_method_t amada38x_coreclk_methods[] = {
	DEVMETHOD(clkdev_write_4,	armada38x_coreclk_write_4),
	DEVMETHOD(clkdev_read_4,	armada38x_coreclk_read_4),
	DEVMETHOD(clkdev_device_lock,	armada38x_coreclk_device_lock),
	DEVMETHOD(clkdev_device_unlock,	armada38x_coreclk_device_unlock),

	DEVMETHOD(device_attach,	armada38x_coreclk_attach),
	DEVMETHOD(device_probe,		armada38x_coreclk_probe),

	DEVMETHOD_END
};

static driver_t armada38x_coreclk_driver = {
	"armada38x_coreclk",
	amada38x_coreclk_methods,
	sizeof(struct armada38x_coreclk_softc),
};

EARLY_DRIVER_MODULE(armada38x_coreclk, simplebus, armada38x_coreclk_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
