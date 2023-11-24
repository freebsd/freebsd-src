/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Axiado Corporation.
 * All rights reserved.
 *
 * This software was developed in part by Philip Paeps under contract for
 * Axiado Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <dev/extres/clk/clk.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "iicbus_if.h"
#include "iicoc.h"

static struct ofw_compat_data compat_data[] = {
	{ "opencores,i2c-ocores",	1 },
	{ "sifive,fu740-c000-i2c",	1 },
	{ "sifive,fu540-c000-i2c",	1 },
	{ "sifive,i2c0",		1 },
	{ NULL,				0 }
};

static struct resource_spec iicoc_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	RESOURCE_SPEC_END
};

static phandle_t
iicoc_get_node(device_t bus, device_t dev)
{

	/* Share controller node with iicbus device. */
	return (ofw_bus_get_node(bus));
}

static int
iicoc_attach(device_t dev)
{
	struct iicoc_softc *sc;
	phandle_t node;
	clk_t clock;
	uint64_t clockfreq;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->sc_mtx, "iicoc", "iicoc", MTX_DEF);

	error = bus_alloc_resources(dev, iicoc_spec, &sc->mem_res);
	if (error) {
		device_printf(dev, "Could not allocate bus resource.\n");
		goto fail;
	}

	node = ofw_bus_get_node(dev);
	sc->reg_shift = 0;
	OF_getencprop(node, "reg-shift", &sc->reg_shift,
	    sizeof(sc->reg_shift));

	error = clk_get_by_ofw_index(dev, 0, 0, &clock);
	if (error) {
		device_printf(dev, "Couldn't get clock\n");
		goto fail;
	}
	error = clk_enable(clock);
	if (error) {
		device_printf(dev, "Couldn't enable clock\n");
		goto fail1;
	}
	error = clk_get_freq(clock, &clockfreq);
	if (error) {
		device_printf(dev, "Couldn't get clock frequency\n");
		goto fail1;
	}
	if (clockfreq > UINT_MAX) {
		device_printf(dev, "Unsupported clock frequency\n");
		goto fail1;
	}
	sc->clockfreq = (u_int)clockfreq;
	sc->i2cfreq = XLP_I2C_FREQ;
	iicoc_init(dev);

	sc->iicbus = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus == NULL) {
		device_printf(dev, "Could not allocate iicbus instance.\n");
		error = ENXIO;
		goto fail1;
	}

	/* Probe and attach the iicbus when interrupts are available. */
	config_intrhook_oneshot((ich_func_t)bus_generic_attach, dev);

	return (0);

fail1:
	clk_disable(clock);

fail:
	bus_release_resources(dev, iicoc_spec, &sc->mem_res);
	mtx_destroy(&sc->sc_mtx);
	return (error);
}

static int
iicoc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "OpenCores I2C master controller");

	return (BUS_PROBE_DEFAULT);
}

static device_method_t iicoc_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe, iicoc_probe),
	DEVMETHOD(device_attach, iicoc_attach),

	/* ofw interface */
	DEVMETHOD(ofw_bus_get_node, iicoc_get_node),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback, iicbus_null_callback),
	DEVMETHOD(iicbus_repeated_start, iicoc_iicbus_repeated_start),
	DEVMETHOD(iicbus_start, iicoc_iicbus_start),
	DEVMETHOD(iicbus_stop, iicoc_iicbus_stop),
	DEVMETHOD(iicbus_reset, iicoc_iicbus_reset),
	DEVMETHOD(iicbus_write, iicoc_iicbus_write),
	DEVMETHOD(iicbus_read, iicoc_iicbus_read),
	DEVMETHOD(iicbus_transfer, iicbus_transfer_gen),

	DEVMETHOD_END
};

static driver_t iicoc_driver = {
	"iicoc",
	iicoc_methods,
	sizeof(struct iicoc_softc),
};

SIMPLEBUS_PNP_INFO(compat_data);
DRIVER_MODULE(iicoc, simplebus, iicoc_driver, 0, 0);
DRIVER_MODULE(ofw_iicbus, iicoc, ofw_iicbus_driver, 0, 0);
MODULE_DEPEND(iicoc, iicbus, 1, 1, 1);
MODULE_DEPEND(iicoc, ofw_iicbus, 1, 1, 1);
