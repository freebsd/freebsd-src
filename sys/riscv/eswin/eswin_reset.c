/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
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

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/syscon/syscon.h>
#include <dev/hwreset/hwreset.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "syscon_if.h"
#include "hwreset_if.h"

struct eswin_rst_softc {
	device_t		dev;
	struct mtx		mtx;
	struct syscon		*syscon;
};

#define	RESET_BLOCK			0x400
#define	RESET_ID_TO_REG(x)		(RESET_BLOCK + (x) * 4)

#define	ERST_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	ERST_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	ERST_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED);
#define	ERST_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED);

#define	ERST_READ(_sc, _reg)		\
    SYSCON_READ_4(sc->syscon, (_reg))
#define	ERST_WRITE(_sc, _reg, _val)	\
    SYSCON_WRITE_4(sc->syscon, (_reg), (_val))

static struct ofw_compat_data compat_data[] = {
	{ "eswin,eic7700-reset",	1 },
	{ NULL,				0 },
};

static int
eswin_rst_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Eswin Reset");

	return (BUS_PROBE_DEFAULT);
}

static int
eswin_rst_attach(device_t dev)
{
	struct eswin_rst_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	error = syscon_get_by_ofw_node(dev, OF_parent(ofw_bus_get_node(dev)),
	    &sc->syscon);
	if (error != 0) {
		device_printf(dev, "Couldn't get syscon handle of parent\n");
		return (error);
	}

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	hwreset_register_ofw_provider(dev);

	return (0);
}

static int
eswin_rst_reset_assert(device_t dev, intptr_t id, bool reset)
{
	struct eswin_rst_softc *sc;
	uint32_t reg;
	uint32_t base;
	uint32_t bit;

	sc = device_get_softc(dev);

	base = RESET_ID_TO_REG(id >> 5);
	bit = id & 0x1f;

	ERST_LOCK(sc);
	reg = ERST_READ(sc, base);
	if (reset)
		reg &= ~(1 << bit);
	else
		reg |= (1 << bit);
	ERST_WRITE(sc, base, reg);
	ERST_UNLOCK(sc);

	return (0);
}

static int
eswin_rst_reset_is_asserted(device_t dev, intptr_t id, bool *reset)
{
	struct eswin_rst_softc *sc;
	uint32_t reg;
	uint32_t base;
	uint32_t bit;

	sc = device_get_softc(dev);

	base = RESET_ID_TO_REG(id >> 5);
	bit = id & 0x1f;

	ERST_LOCK(sc);
	reg = ERST_READ(sc, base);
	*reset = (reg & (1 << bit)) == 0;
	ERST_UNLOCK(sc);

	return (0);
}

static int
eswin_rst_map(device_t provider_dev, phandle_t xref, int ncells,
    pcell_t *cells, intptr_t *id)
{

	KASSERT(ncells == 2, ("wrong ncells"));

	*id = cells[0] << 5;
	*id |= ilog2(cells[1]);

	return (0);
}

static device_method_t eswin_rst_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		eswin_rst_probe),
	DEVMETHOD(device_attach,	eswin_rst_attach),

	/* Reset interface. */
	DEVMETHOD(hwreset_assert,	eswin_rst_reset_assert),
	DEVMETHOD(hwreset_is_asserted,	eswin_rst_reset_is_asserted),
	DEVMETHOD(hwreset_map,		eswin_rst_map),

	DEVMETHOD_END
};

static driver_t eswin_rst_driver = {
	"eswin_rst",
	eswin_rst_methods,
	sizeof(struct eswin_rst_softc)
};

EARLY_DRIVER_MODULE(eswin_rst, simplebus, eswin_rst_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LATE);
