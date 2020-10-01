/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2015 Michal Meloun
 * All rights reserved.
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

/*
 * This is a generic syscon driver, whose purpose is to provide access to
 * various unrelated bits packed in a single register space. It is usually used
 * as a fallback to more specific driver, but works well enough for simple
 * access.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "syscon_if.h"
#include "syscon.h"
#include "syscon_generic.h"

MALLOC_DECLARE(M_SYSCON);

static uint32_t syscon_generic_unlocked_read_4(struct syscon *syscon,
    bus_size_t offset);
static int syscon_generic_unlocked_write_4(struct syscon *syscon,
    bus_size_t offset, uint32_t val);
static int syscon_generic_unlocked_modify_4(struct syscon *syscon,
    bus_size_t offset, uint32_t clear_bits, uint32_t set_bits);
static int syscon_generic_detach(device_t dev);
/*
 * Generic syscon driver (FDT)
 */
static struct ofw_compat_data compat_data[] = {
	{"syscon",	1},
	{NULL,		0}
};

#define SYSCON_LOCK(_sc)		mtx_lock_spin(&(_sc)->mtx)
#define	SYSCON_UNLOCK(_sc)		mtx_unlock_spin(&(_sc)->mtx)
#define SYSCON_LOCK_INIT(_sc)		mtx_init(&(_sc)->mtx,		\
	    device_get_nameunit((_sc)->dev), "syscon", MTX_SPIN)
#define SYSCON_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->mtx);
#define SYSCON_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED);
#define SYSCON_ASSERT_UNLOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_NOTOWNED);

static syscon_method_t syscon_generic_methods[] = {
	SYSCONMETHOD(syscon_unlocked_read_4,  syscon_generic_unlocked_read_4),
	SYSCONMETHOD(syscon_unlocked_write_4, syscon_generic_unlocked_write_4),
	SYSCONMETHOD(syscon_unlocked_modify_4, syscon_generic_unlocked_modify_4),

	SYSCONMETHOD_END
};
DEFINE_CLASS_1(syscon_generic, syscon_generic_class, syscon_generic_methods,
    0, syscon_class);

static uint32_t
syscon_generic_unlocked_read_4(struct syscon *syscon, bus_size_t offset)
{
	struct syscon_generic_softc *sc;
	uint32_t val;

	sc = device_get_softc(syscon->pdev);
	SYSCON_ASSERT_LOCKED(sc);
	val = bus_read_4(sc->mem_res, offset);
	return (val);
}

static int
syscon_generic_unlocked_write_4(struct syscon *syscon, bus_size_t offset, uint32_t val)
{
	struct syscon_generic_softc *sc;

	sc = device_get_softc(syscon->pdev);
	SYSCON_ASSERT_LOCKED(sc);
	bus_write_4(sc->mem_res, offset, val);
	return (0);
}

static int
syscon_generic_unlocked_modify_4(struct syscon *syscon, bus_size_t offset,
    uint32_t clear_bits, uint32_t set_bits)
{
	struct syscon_generic_softc *sc;
	uint32_t val;

	sc = device_get_softc(syscon->pdev);
	SYSCON_ASSERT_LOCKED(sc);
	val = bus_read_4(sc->mem_res, offset);
	val &= ~clear_bits;
	val |= set_bits;
	bus_write_4(sc->mem_res, offset, val);
	return (0);
}

static void
syscon_generic_lock(device_t dev)
{
	struct syscon_generic_softc *sc;

	sc = device_get_softc(dev);
	SYSCON_LOCK(sc);
}

static void
syscon_generic_unlock(device_t dev)
{
	struct syscon_generic_softc *sc;

	sc = device_get_softc(dev);
	SYSCON_UNLOCK(sc);
}

static int
syscon_generic_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "syscon");
	if (!bootverbose)
		device_quiet(dev);

	return (BUS_PROBE_GENERIC);
}

static int
syscon_generic_attach(device_t dev)
{
	struct syscon_generic_softc *sc;
	int rid, rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	rid = 0;

	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resource\n");
		return (ENXIO);
	}

	SYSCON_LOCK_INIT(sc);
	sc->syscon = syscon_create_ofw_node(dev, &syscon_generic_class,
		ofw_bus_get_node(dev));
	if (sc->syscon == NULL) {
		device_printf(dev, "Failed to create/register syscon\n");
		syscon_generic_detach(dev);
		return (ENXIO);
	}
	if (ofw_bus_is_compatible(dev, "simple-bus")) {
		rv = simplebus_attach_impl(sc->dev);
		if (rv != 0) {
			device_printf(dev, "Failed to create simplebus\n");
			syscon_generic_detach(dev);
			return (ENXIO);
		}
		sc->simplebus_attached = true;
	}

	return (bus_generic_attach(dev));
}

static int
syscon_generic_detach(device_t dev)
{
	struct syscon_generic_softc *sc;

	sc = device_get_softc(dev);
	if (sc->syscon != NULL) {
		syscon_unregister(sc->syscon);
		free(sc->syscon, M_SYSCON);
	}
	if (sc->simplebus_attached)
		simplebus_detach(dev);
	SYSCON_LOCK_DESTROY(sc);

	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
	return (0);
}

static device_method_t syscon_generic_dmethods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		syscon_generic_probe),
	DEVMETHOD(device_attach,	syscon_generic_attach),
	DEVMETHOD(device_detach,	syscon_generic_detach),

	DEVMETHOD(syscon_device_lock,	syscon_generic_lock),
	DEVMETHOD(syscon_device_unlock,	syscon_generic_unlock),

	DEVMETHOD_END
};

DEFINE_CLASS_1(syscon_generic_dev, syscon_generic_driver, syscon_generic_dmethods,
    sizeof(struct syscon_generic_softc), simplebus_driver);
static devclass_t syscon_generic_devclass;

EARLY_DRIVER_MODULE(syscon_generic, simplebus, syscon_generic_driver,
    syscon_generic_devclass, 0, 0, BUS_PASS_DEFAULT);
MODULE_VERSION(syscon_generic, 1);
