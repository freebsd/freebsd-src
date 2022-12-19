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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "mmio_sram_if.h"

#include "scmi.h"

struct shmem_softc {
	device_t		dev;
	device_t		parent;
	int			reg;
};

static int
shmem_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "arm,scmi-shmem"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM SCMI Shared Memory driver");

	return (BUS_PROBE_DEFAULT);
}

static int
shmem_attach(device_t dev)
{
	struct shmem_softc *sc;
	phandle_t node;
	int reg;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->parent = device_get_parent(dev);

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

	OF_getencprop(node, "reg", &reg, sizeof(reg));

	dprintf("%s: reg %x\n", __func__, reg);

	sc->reg = reg;

	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
}

static int
shmem_detach(device_t dev)
{

	return (0);
}

void
scmi_shmem_read(device_t dev, bus_size_t offset, void *buf, bus_size_t len)
{
	struct shmem_softc *sc;
	uint8_t *addr;
	int i;

	sc = device_get_softc(dev);

	addr = (uint8_t *)buf;

	for (i = 0; i < len; i++)
		addr[i] = MMIO_SRAM_READ_1(sc->parent, sc->reg + offset + i);
}

void
scmi_shmem_write(device_t dev, bus_size_t offset, const void *buf,
    bus_size_t len)
{
	struct shmem_softc *sc;
	const uint8_t *addr;
	int i;

	sc = device_get_softc(dev);

	addr = (const uint8_t *)buf;

	for (i = 0; i < len; i++)
		MMIO_SRAM_WRITE_1(sc->parent, sc->reg + offset + i, addr[i]);
}

static device_method_t shmem_methods[] = {
	DEVMETHOD(device_probe,		shmem_probe),
	DEVMETHOD(device_attach,	shmem_attach),
	DEVMETHOD(device_detach,	shmem_detach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(shmem, shmem_driver, shmem_methods, sizeof(struct shmem_softc),
    simplebus_driver);

EARLY_DRIVER_MODULE(shmem, mmio_sram, shmem_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(scmi, 1);
