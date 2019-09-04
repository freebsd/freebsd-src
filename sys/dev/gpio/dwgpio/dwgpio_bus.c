/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "dwgpio_if.h"

struct dwgpiobus_softc {
	struct simplebus_softc	simplebus_sc;
	device_t		dev;
	struct resource		*res[1];
};

static struct resource_spec dwgpio_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static int
dwgpiobus_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "snps,dw-apb-gpio"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "Synopsys® DesignWare® APB GPIO BUS");

	return (BUS_PROBE_DEFAULT);
}

static int
dwgpiobus_attach(device_t dev)
{
	struct dwgpiobus_softc *sc;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

	if (bus_alloc_resources(dev, dwgpio_spec, sc->res)) {
		device_printf(dev, "Could not allocate resources.\n");
		return (ENXIO);
	}

	simplebus_init(dev, node);

	/*
	 * Allow devices to identify.
	 */
	bus_generic_probe(dev);

	/*
	 * Now walk the OFW tree and attach top-level devices.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node))
		simplebus_add_device(dev, node, 0, NULL, -1, NULL);

	return (bus_generic_attach(dev));
}

static int
dwgpiobus_detach(device_t dev)
{
	struct dwgpiobus_softc *sc;

	sc = device_get_softc(dev);

	bus_release_resources(dev, dwgpio_spec, sc->res);

	return (0);
}

static int
dwgpiobus_write(device_t dev, bus_size_t offset, int val)
{
	struct dwgpiobus_softc *sc;

	sc = device_get_softc(dev);

	bus_write_4(sc->res[0], offset, val);

	return (0);
};

static int
dwgpiobus_read(device_t dev, bus_size_t offset)
{
	struct dwgpiobus_softc *sc;
	int val;

	sc = device_get_softc(dev);

	val = bus_read_4(sc->res[0], offset);

	return (val);
};

static device_method_t dwgpiobus_methods[] = {
	DEVMETHOD(device_probe,		dwgpiobus_probe),
	DEVMETHOD(device_attach,	dwgpiobus_attach),
	DEVMETHOD(device_detach,	dwgpiobus_detach),

	DEVMETHOD(dwgpio_write,		dwgpiobus_write),
	DEVMETHOD(dwgpio_read,		dwgpiobus_read),

	DEVMETHOD_END
};

DEFINE_CLASS_1(dwgpiobus, dwgpiobus_driver, dwgpiobus_methods,
    sizeof(struct dwgpiobus_softc), simplebus_driver);

static devclass_t dwgpiobus_devclass;

EARLY_DRIVER_MODULE(dwgpiobus, simplebus, dwgpiobus_driver, dwgpiobus_devclass,
    0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(dwgpiobus, 1);
