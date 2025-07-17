/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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
/*
 * ACT8846 PMIC driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/sx.h>

#include <machine/bus.h>

#include <dev/regulator/regulator.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/pmic/act8846.h>

#include "regdev_if.h"

static struct ofw_compat_data compat_data[] = {
	{"active-semi,act8846",	1},
	{NULL,			0}
};

#define	LOCK(_sc)		sx_xlock(&(_sc)->lock)
#define	UNLOCK(_sc)		sx_xunlock(&(_sc)->lock)
#define	LOCK_INIT(_sc)		sx_init(&(_sc)->lock, "act8846")
#define	LOCK_DESTROY(_sc)	sx_destroy(&(_sc)->lock);
#define	ASSERT_LOCKED(_sc)	sx_assert(&(_sc)->lock, SA_XLOCKED);
#define	ASSERT_UNLOCKED(_sc)	sx_assert(&(_sc)->lock, SA_UNLOCKED);


/*
 * Raw register access function.
 */
int
act8846_read(struct act8846_softc *sc, uint8_t reg, uint8_t *val)
{
	uint8_t addr;
	int rv;
	struct iic_msg msgs[2] = {
		{0, IIC_M_WR, 1, &addr},
		{0, IIC_M_RD, 1, val},
	};

	msgs[0].slave = sc->bus_addr;
	msgs[1].slave = sc->bus_addr;
	addr = reg;

	rv = iicbus_transfer_excl(sc->dev, msgs, 2, IIC_INTRWAIT);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when reading reg 0x%02X, rv: %d\n", reg,  rv);
		return (EIO);
	}

	return (0);
}

int act8846_read_buf(struct act8846_softc *sc, uint8_t reg, uint8_t *buf,
    size_t size)
{
	uint8_t addr;
	int rv;
	struct iic_msg msgs[2] = {
		{0, IIC_M_WR, 1, &addr},
		{0, IIC_M_RD, size, buf},
	};

	msgs[0].slave = sc->bus_addr;
	msgs[1].slave = sc->bus_addr;
	addr = reg;

	rv = iicbus_transfer_excl(sc->dev, msgs, 2, IIC_INTRWAIT);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when reading reg 0x%02X, rv: %d\n", reg,  rv);
		return (EIO);
	}

	return (0);
}

int
act8846_write(struct act8846_softc *sc, uint8_t reg, uint8_t val)
{
	uint8_t data[2];
	int rv;

	struct iic_msg msgs[1] = {
		{0, IIC_M_WR, 2, data},
	};

	msgs[0].slave = sc->bus_addr;
	data[0] = reg;
	data[1] = val;

	rv = iicbus_transfer_excl(sc->dev, msgs, 1, IIC_INTRWAIT);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when writing reg 0x%02X, rv: %d\n", reg, rv);
		return (EIO);
	}
	return (0);
}

int act8846_write_buf(struct act8846_softc *sc, uint8_t reg, uint8_t *buf,
    size_t size)
{
	uint8_t data[1];
	int rv;
	struct iic_msg msgs[2] = {
		{0, IIC_M_WR, 1, data},
		{0, IIC_M_WR | IIC_M_NOSTART, size, buf},
	};

	msgs[0].slave = sc->bus_addr;
	msgs[1].slave = sc->bus_addr;
	data[0] = reg;

	rv = iicbus_transfer_excl(sc->dev, msgs, 2, IIC_INTRWAIT);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when writing reg 0x%02X, rv: %d\n", reg, rv);
		return (EIO);
	}
	return (0);
}

int
act8846_modify(struct act8846_softc *sc, uint8_t reg, uint8_t clear, uint8_t set)
{
	uint8_t val;
	int rv;

	rv = act8846_read(sc, reg, &val);
	if (rv != 0)
		return (rv);

	val &= ~clear;
	val |= set;

	rv = act8846_write(sc, reg, val);
	if (rv != 0)
		return (rv);

	return (0);
}

static int
act8846_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "ACT8846 PMIC");
	return (BUS_PROBE_DEFAULT);
}

static int
act8846_attach(device_t dev)
{
	struct act8846_softc *sc;
	int rv;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->bus_addr = iicbus_get_addr(dev);
	node = ofw_bus_get_node(sc->dev);
	rv = 0;
	LOCK_INIT(sc);


	rv = act8846_regulator_attach(sc, node);
	if (rv != 0)
		goto fail;

	bus_attach_children(dev);
	return (0);

fail:
	LOCK_DESTROY(sc);
	return (rv);
}

static int
act8846_detach(device_t dev)
{
	struct act8846_softc *sc;
	int error;

	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);

	sc = device_get_softc(dev);
	LOCK_DESTROY(sc);

	return (0);
}

static device_method_t act8846_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		act8846_probe),
	DEVMETHOD(device_attach,	act8846_attach),
	DEVMETHOD(device_detach,	act8846_detach),

	/* Regdev interface */
	DEVMETHOD(regdev_map,		act8846_regulator_map),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(act8846_pmu, act8846_driver, act8846_methods,
    sizeof(struct act8846_softc));
EARLY_DRIVER_MODULE(act8846_pmic, iicbus, act8846_driver,
    NULL, NULL, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
MODULE_VERSION(act8846_pmic, 1);
MODULE_DEPEND(act8846_pmic, iicbus, IICBUS_MINVER, IICBUS_PREFVER,
    IICBUS_MAXVER);
IICBUS_FDT_PNP_INFO(compat_data);
