/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "mmio_sram_if.h"

#define	dprintf(fmt, ...)

static struct resource_spec mmio_sram_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct mmio_sram_softc {
	struct simplebus_softc	simplebus_sc;
	struct resource		*res[1];
	device_t		dev;
};

static int
mmio_sram_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "mmio-sram"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "MMIO SRAM");

	return (BUS_PROBE_DEFAULT);
}

static int
mmio_sram_attach(device_t dev)
{
	struct mmio_sram_softc *sc;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, mmio_sram_spec, sc->res) != 0) {
		device_printf(dev, "Can't allocate resources for device.\n");
		return (ENXIO);
	}

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

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
mmio_sram_detach(device_t dev)
{
	struct mmio_sram_softc *sc;

	sc = device_get_softc(dev);

	bus_release_resources(dev, mmio_sram_spec, sc->res);

	return (0);
}

static uint8_t
mmio_sram_read_1(device_t dev, bus_size_t offset)
{
	struct mmio_sram_softc *sc;

	sc = device_get_softc(dev);

	dprintf("%s: reading from %lx\n", __func__, offset);

	return (bus_read_1(sc->res[0], offset));
}

static void
mmio_sram_write_1(device_t dev, bus_size_t offset, uint8_t val)
{
	struct mmio_sram_softc *sc;

	sc = device_get_softc(dev);

	dprintf("%s: writing to %lx val %x\n", __func__, offset, val);

	bus_write_1(sc->res[0], offset, val);
}

static device_method_t mmio_sram_methods[] = {
	/* Device Interface */
	DEVMETHOD(device_probe,		mmio_sram_probe),
	DEVMETHOD(device_attach,	mmio_sram_attach),
	DEVMETHOD(device_detach,	mmio_sram_detach),

	/* MMIO interface */
	DEVMETHOD(mmio_sram_read_1,	mmio_sram_read_1),
	DEVMETHOD(mmio_sram_write_1,	mmio_sram_write_1),
	DEVMETHOD_END
};

DEFINE_CLASS_1(mmio_sram, mmio_sram_driver, mmio_sram_methods,
    sizeof(struct mmio_sram_softc), simplebus_driver);

EARLY_DRIVER_MODULE(mmio_sram, simplebus, mmio_sram_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(mmio_sram, 1);
