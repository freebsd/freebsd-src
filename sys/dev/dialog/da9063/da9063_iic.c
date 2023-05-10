/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Jessica Clarke <jrtc27@FreeBSD.org>
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

/* Dialog Semiconductor DA9063 PMIC, 2-WIRE */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/dialog/da9063/da9063reg.h>
#include <dev/fdt/simplebus.h>
#include <dev/iicbus/iiconf.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "da9063_if.h"

#define	DA9063_IIC_PAGE_SHIFT	8
#define	DA9063_IIC_PAGE_SIZE	(1 << DA9063_IIC_PAGE_SHIFT)
#define	DA9063_IIC_PAGE(_a)	((_a) >> DA9063_IIC_PAGE_SHIFT)
#define	DA9063_IIC_PAGE_OFF(_a)	((_a) & (DA9063_IIC_PAGE_SIZE - 1))
#define	DA9063_IIC_ADDR(_p, _o)	(((_p) << DA9063_IIC_PAGE_SHIFT) | (_o))

/*
 * For 2-WIRE (I2C) operation pages are 256 registers but PAGE_CON is in units
 * of 128 registers with the LSB ignored so scale the page when writing to it.
 */
#define	DA9063_IIC_PAGE_CON_REG_PAGE_SHIFT	1

struct da9063_iic_softc {
	struct simplebus_softc	simplebus_sc;
	device_t		dev;
	struct mtx		mtx;
	uint8_t			page;
};

#define	DA9063_IIC_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define	DA9063_IIC_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	DA9063_IIC_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->mtx, MA_OWNED);
#define	DA9063_IIC_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED);

static struct ofw_compat_data compat_data[] = {
	{ "dlg,da9063",	1 },
	{ NULL,		0 }
};

static int
da9063_iic_select_page(struct da9063_iic_softc *sc, uint16_t page)
{
	uint8_t reg;
	int error;

	DA9063_IIC_ASSERT_LOCKED(sc);

	if (page == sc->page)
		return (0);

	error = iicdev_readfrom(sc->dev, DA9063_PAGE_CON, &reg, 1, IIC_WAIT);
	if (error != 0)
		return (iic2errno(error));

	reg &= ~(DA9063_PAGE_CON_REG_PAGE_MASK <<
	    DA9063_PAGE_CON_REG_PAGE_SHIFT);
	reg |= (page << DA9063_IIC_PAGE_CON_REG_PAGE_SHIFT) <<
	    DA9063_PAGE_CON_REG_PAGE_SHIFT;

	error = iicdev_writeto(sc->dev, DA9063_PAGE_CON, &reg, 1, IIC_WAIT);
	if (error != 0)
		return (iic2errno(error));

	sc->page = page;

	return (0);
}

static int
da9063_iic_read(device_t dev, uint16_t addr, uint8_t *val)
{
	struct da9063_iic_softc *sc;
	int error;

	sc = device_get_softc(dev);

	DA9063_IIC_LOCK(sc);

	error = da9063_iic_select_page(sc, DA9063_IIC_PAGE(addr));
	if (error != 0)
		goto error;

	error = iicdev_readfrom(dev, DA9063_IIC_PAGE_OFF(addr), val, 1,
	    IIC_WAIT);
	if (error != 0)
		error = iic2errno(error);

error:
	DA9063_IIC_UNLOCK(sc);

	return (error);
}

static int
da9063_iic_write(device_t dev, uint16_t addr, uint8_t val)
{
	struct da9063_iic_softc *sc;
	int error;

	sc = device_get_softc(dev);

	DA9063_IIC_LOCK(sc);

	error = da9063_iic_select_page(sc, DA9063_IIC_PAGE(addr));
	if (error != 0)
		goto error;

	error = iicdev_writeto(dev, DA9063_IIC_PAGE_OFF(addr), &val, 1,
	    IIC_WAIT);
	if (error != 0)
		error = iic2errno(error);

error:
	DA9063_IIC_UNLOCK(sc);

	return (error);
}

static int
da9063_iic_modify(device_t dev, uint16_t addr, uint8_t clear_mask,
    uint8_t set_mask)
{
	struct da9063_iic_softc *sc;
	uint8_t reg;
	int error;

	sc = device_get_softc(dev);

	DA9063_IIC_LOCK(sc);

	error = da9063_iic_select_page(sc, DA9063_IIC_PAGE(addr));
	if (error != 0)
		goto error;

	error = iicdev_readfrom(dev, DA9063_IIC_PAGE_OFF(addr), &reg, 1,
	    IIC_WAIT);
	if (error != 0) {
		error = iic2errno(error);
		goto error;
	}

	reg &= ~clear_mask;
	reg |= set_mask;

	error = iicdev_writeto(dev, DA9063_IIC_PAGE_OFF(addr), &reg, 1,
	    IIC_WAIT);
	if (error != 0)
		error = iic2errno(error);

error:
	DA9063_IIC_UNLOCK(sc);

	return (error);
}

static int
da9063_iic_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Dialog DA9063 PMIC");

	return (BUS_PROBE_DEFAULT);
}

static int
da9063_iic_attach(device_t dev)
{
	struct da9063_iic_softc *sc;
	uint8_t reg;
	int error;

	sc = device_get_softc(dev);

	sc->dev = dev;

	error = iicdev_readfrom(dev, DA9063_PAGE_CON, &reg, 1, IIC_WAIT);
	if (error != 0)
		return (iic2errno(error));

	sc->page = ((reg >> DA9063_PAGE_CON_REG_PAGE_SHIFT) &
	    DA9063_PAGE_CON_REG_PAGE_MASK) >> DA9063_IIC_PAGE_CON_REG_PAGE_SHIFT;
	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	sc->simplebus_sc.flags |= SB_FLAG_NO_RANGES;

	return (simplebus_attach(dev));
}

static int
da9063_iic_detach(device_t dev)
{
	struct da9063_iic_softc *sc;
	int error;

	sc = device_get_softc(dev);

	error = simplebus_detach(dev);
	if (error != 0)
		return (error);

	mtx_destroy(&sc->mtx);

	return (0);
}

static device_method_t da9063_iic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		da9063_iic_probe),
	DEVMETHOD(device_attach,	da9063_iic_attach),
	DEVMETHOD(device_detach,	da9063_iic_detach),

	/* DA9063 interface */
	DEVMETHOD(da9063_read,		da9063_iic_read),
	DEVMETHOD(da9063_write,		da9063_iic_write),
	DEVMETHOD(da9063_modify,	da9063_iic_modify),

	DEVMETHOD_END
};

DEFINE_CLASS_1(da9063_pmic, da9063_iic_driver, da9063_iic_methods,
    sizeof(struct da9063_iic_softc), simplebus_driver);

DRIVER_MODULE(da9063_iic, iicbus, da9063_iic_driver, NULL, NULL);
