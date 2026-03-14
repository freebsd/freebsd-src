/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Perdixky <3293789706@qq.com>
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
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/clk/broadcom/bcm_cprman.h>
#include <dev/clk/clk.h>

#include "clkdev_if.h"

#if 0
#define dprintf(fmt, ...) device_printf(dev, "%s: " fmt, __func__, __VA_ARGS__)
#else
#define dprintf(...)
#endif

static struct resource_spec bcm_cprman_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ -1, 0 }
};

#define CPR_READ4(sc, reg)	 bus_read_4((sc)->res, (reg))
#define CPR_WRITE4(sc, reg, val) bus_write_4((sc)->res, (reg), (val))

static int
bcm_cprman_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct bcm_cprman_softc *sc;

	sc = device_get_softc(dev);
	dprintf("offset=%lx write %x\n", addr, val);
	CPR_WRITE4(sc, addr, val);
	return (0);
}

static int
bcm_cprman_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct bcm_cprman_softc *sc;

	sc = device_get_softc(dev);
	*val = CPR_READ4(sc, addr);
	dprintf("offset=%lx read %x\n", addr, *val);
	return (0);
}

static int
bcm_cprman_modify_4(device_t dev, bus_addr_t addr, uint32_t clr, uint32_t set)
{
	struct bcm_cprman_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	dprintf("offset=%lx clr: %x set: %x\n", addr, clr, set);
	reg = CPR_READ4(sc, addr);
	reg &= ~clr;
	reg |= set;
	return (bcm_cprman_write_4(dev, addr, reg));
}

static void
bcm_cprman_device_lock(device_t dev)
{
	struct bcm_cprman_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
bcm_cprman_device_unlock(device_t dev)
{
	struct bcm_cprman_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

int
bcm_cprman_attach(device_t dev)
{
	struct bcm_cprman_softc *sc;
	int error;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, bcm_cprman_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_SPIN);

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "cannot create clkdom\n");
		return (ENXIO);
	}

	for (i = 0; i < nitems(sc->clks); i++) {
		error = bcm_clk_periph_register(sc->clkdom, &sc->clks[i]);
		if (error != 0) {
			device_printf(dev, "cannot register periph clks\n");
			return (ENXIO);
		}
	}

	if (clkdom_finit(sc->clkdom) != 0)
		panic("cannot finalize clkdom initialization");

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);
}

static device_method_t bcm_cprman_methods[] = {
	/* clkdev interface: called by clknode layer to access hardware */
	DEVMETHOD(clkdev_write_4,		bcm_cprman_write_4),
	DEVMETHOD(clkdev_read_4,		bcm_cprman_read_4),
	DEVMETHOD(clkdev_modify_4,		bcm_cprman_modify_4),
	DEVMETHOD(clkdev_device_lock,	bcm_cprman_device_lock),
	DEVMETHOD(clkdev_device_unlock,	bcm_cprman_device_unlock),
	DEVMETHOD_END
};

/*
 * Base class: holds the clkdev methods.  Chip-specific drivers
 * (e.g. bcm2835_cprman) inherit from this via DEFINE_CLASS_1 and
 * add their own device_probe / device_attach on top.
 */
DEFINE_CLASS_0(bcm_cprman, bcm_cprman_driver, bcm_cprman_methods,
    sizeof(struct bcm_cprman_softc));
