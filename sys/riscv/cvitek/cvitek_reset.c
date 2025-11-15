/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Bojan Novković <bnovkov@FreeBSD.org
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
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/hwreset/hwreset.h>
#include <dev/syscon/syscon.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "syscon_if.h"
#include "hwreset_if.h"

struct cvitek_reset_softc {
	device_t		dev;
	struct mtx		mtx;
	struct syscon	*syscon;
};

static int
cvitek_reset_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "cvitek,reset")) {
		device_set_desc(dev, "CVITEK reset controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
cvitek_reset_attach(device_t dev)
{
	struct cvitek_reset_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	error = syscon_get_by_ofw_property(dev, ofw_bus_get_node(dev),
	    "syscon", &sc->syscon);
	if (error != 0) {
		device_printf(dev, "Couldn't get syscon handle\n");
		return (error);
	}
	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	hwreset_register_ofw_provider(dev);

	return (0);
}

static int
cvitek_reset_assert(device_t dev, intptr_t id, bool reset)
{
	struct cvitek_reset_softc *sc;
	uint32_t offset, val;
	uint32_t bit;

	sc = device_get_softc(dev);
	bit = id % 32;
	offset = id / 32;

	mtx_lock(&sc->mtx);
	val = SYSCON_READ_4(sc->syscon, offset);
	if (reset)
		val &= ~(1 << bit);
	else
		val |= (1 << bit);
	SYSCON_WRITE_4(sc->syscon, offset, val);
	mtx_unlock(&sc->mtx);

	return (0);
}

static device_method_t cvitek_reset_methods[] = {
	DEVMETHOD(device_probe,		cvitek_reset_probe),
	DEVMETHOD(device_attach,	cvitek_reset_attach),

	DEVMETHOD(hwreset_assert,	cvitek_reset_assert),

	DEVMETHOD_END
};

static driver_t cvitek_reset_driver = {
	"cvitek_reset",
	cvitek_reset_methods,
	sizeof(struct cvitek_reset_softc)
};

DRIVER_MODULE(cvitek_reset, simplebus, cvitek_reset_driver, 0, 0);
