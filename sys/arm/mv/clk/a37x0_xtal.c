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
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/syscon/syscon.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "syscon_if.h"

#define BIT(x)			(1 << (x))

#define NB_GPIO1_PIN_LT_L	0x8
#define NB_GPIO1_MPP1_9		BIT(9)

struct a37x0_xtal_softc {
	device_t 		dev;
	struct clkdom		*clkdom;
};

static int a37x0_xtal_attach(device_t dev);
static int a37x0_xtal_detach(device_t dev);
static int a37x0_xtal_probe(device_t dev);

static device_method_t a37x0_xtal_methods [] = {
	DEVMETHOD(device_probe, 	a37x0_xtal_probe),
	DEVMETHOD(device_attach, 	a37x0_xtal_attach),
	DEVMETHOD(device_detach, 	a37x0_xtal_detach),
	DEVMETHOD_END
};

static driver_t a37x0_xtal_driver = {
	"a37x0-xtal",
	a37x0_xtal_methods,
	sizeof(struct a37x0_xtal_softc)
};

EARLY_DRIVER_MODULE(a37x0_xtal, simplebus, a37x0_xtal_driver, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_EARLY);

static int
a37x0_xtal_attach(device_t dev)
{
	struct a37x0_xtal_softc *sc;
	struct clk_fixed_def def;
	struct syscon *syscon;
	uint32_t reg;
	int error;

	sc = device_get_softc(dev);

	def.clkdef.name = "armada-3700-xtal";
	def.clkdef.parent_names = NULL;
	def.clkdef.parent_cnt = 0;
	def.clkdef.id = 1;
	def.mult = 0;
	def.div = 0;

	if (SYSCON_GET_HANDLE(dev, &syscon) != 0 || syscon == NULL){
		device_printf(dev, "Cannot get syscon driver handle\n");
		return (ENXIO);
	}

	reg = SYSCON_READ_4(syscon, NB_GPIO1_PIN_LT_L);
	if (reg & NB_GPIO1_MPP1_9)
		def.freq = 40000000;
	else
		def.freq = 25000000;

	sc->clkdom = clkdom_create(dev);
	error = clknode_fixed_register(sc->clkdom, &def);
	if (error){
		device_printf(dev, "Cannot register clock node\n");
		return (ENXIO);
	}

	error = clkdom_finit(sc->clkdom);
	if (error){
		device_printf(dev, "Cannot finalize clock domain initialization\n");
		return (ENXIO);
	}

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);
}

static int
a37x0_xtal_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "marvell,armada-3700-xtal-clock"))
		return (ENXIO);

	device_set_desc(dev, "Marvell Armada 3700 Oscillator");
	return (BUS_PROBE_DEFAULT);
}

static int
a37x0_xtal_detach(device_t dev)
{

	return (EBUSY);
}
