/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sx.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/ofw/ofw_bus.h>

#include "clock_if.h"
#include "max77620.h"
#define	MAX77620_RTC_INT	0x00
#define	MAX77620_RTC_INTM	0x01
#define	MAX77620_RTC_CONTROLM	0x02
#define	MAX77620_RTC_CONTROL	0x03
#define  RTC_CONTROL_MODE_24		(1 << 1)
#define  RTC_CONTROL_BCD_EN		(1 << 0)

#define	MAX77620_RTC_UPDATE0	0x04
#define	 RTC_UPDATE0_RTC_RBUDR		(1 << 4)
#define	 RTC_UPDATE0_RTC_UDR		(1 << 0)

#define	MAX77620_WTSR_SMPL_CNTL	0x06
#define	MAX77620_RTC_SEC	0x07
#define	MAX77620_RTC_MIN	0x08
#define	MAX77620_RTC_HOUR	0x09
#define	MAX77620_RTC_WEEKDAY	0x0A
#define	MAX77620_RTC_MONTH	0x0B
#define	MAX77620_RTC_YEAR	0x0C
#define	MAX77620_RTC_DATE	0x0D
#define	MAX77620_ALARM1_SEC	0x0E
#define	MAX77620_ALARM1_MIN	0x0F
#define	MAX77620_ALARM1_HOUR	0x10
#define	MAX77620_ALARM1_WEEKDAY	0x11
#define	MAX77620_ALARM1_MONTH	0x12
#define	MAX77620_ALARM1_YEAR	0x13
#define	MAX77620_ALARM1_DATE	0x14
#define	MAX77620_ALARM2_SEC	0x15
#define	MAX77620_ALARM2_MIN	0x16
#define	MAX77620_ALARM2_HOUR	0x17
#define	MAX77620_ALARM2_WEEKDAY	0x18
#define	MAX77620_ALARM2_MONTH	0x19
#define	MAX77620_ALARM2_YEAR	0x1A
#define	MAX77620_ALARM2_DATE	0x1B

#define	MAX77620_RTC_START_YEAR	2000
#define MAX77620_RTC_I2C_ADDR	0x68

#define	LOCK(_sc)		sx_xlock(&(_sc)->lock)
#define	UNLOCK(_sc)		sx_xunlock(&(_sc)->lock)
#define	LOCK_INIT(_sc)		sx_init(&(_sc)->lock, "max77620_rtc")
#define	LOCK_DESTROY(_sc)	sx_destroy(&(_sc)->lock);

struct max77620_rtc_softc {
	device_t			dev;
	struct sx			lock;
	int				bus_addr;
};

/*
 * Raw register access function.
 */
static int
max77620_rtc_read(struct max77620_rtc_softc *sc, uint8_t reg, uint8_t *val)
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

	rv = iicbus_transfer(sc->dev, msgs, 2);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when reading reg 0x%02X, rv: %d\n", reg,  rv);
		return (EIO);
	}

	return (0);
}

static int
max77620_rtc_read_buf(struct max77620_rtc_softc *sc, uint8_t reg,
    uint8_t *buf, size_t size)
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

	rv = iicbus_transfer(sc->dev, msgs, 2);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when reading reg 0x%02X, rv: %d\n", reg,  rv);
		return (EIO);
	}

	return (0);
}

static int
max77620_rtc_write(struct max77620_rtc_softc *sc, uint8_t reg, uint8_t val)
{
	uint8_t data[2];
	int rv;

	struct iic_msg msgs[1] = {
		{0, IIC_M_WR, 2, data},
	};

	msgs[0].slave = sc->bus_addr;
	data[0] = reg;
	data[1] = val;

	rv = iicbus_transfer(sc->dev, msgs, 1);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when writing reg 0x%02X, rv: %d\n", reg, rv);
		return (EIO);
	}
	return (0);
}

static int
max77620_rtc_write_buf(struct max77620_rtc_softc *sc, uint8_t reg, uint8_t *buf,
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

	rv = iicbus_transfer(sc->dev, msgs, 2);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when writing reg 0x%02X, rv: %d\n", reg, rv);
		return (EIO);
	}
	return (0);
}

static int
max77620_rtc_modify(struct max77620_rtc_softc *sc, uint8_t reg, uint8_t clear,
    uint8_t set)
{
	uint8_t val;
	int rv;

	rv = max77620_rtc_read(sc, reg, &val);
	if (rv != 0)
		return (rv);

	val &= ~clear;
	val |= set;

	rv = max77620_rtc_write(sc, reg, val);
	if (rv != 0)
		return (rv);

	return (0);
}

static int
max77620_rtc_update(struct max77620_rtc_softc *sc, bool for_read)
{
	uint8_t reg;
	int rv;

	reg = for_read ? RTC_UPDATE0_RTC_RBUDR: RTC_UPDATE0_RTC_UDR;
	rv = max77620_rtc_modify(sc, MAX77620_RTC_UPDATE0, reg, reg);
	if (rv != 0)
		return (rv);

	DELAY(16000);
	return (rv);
}

static int
max77620_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct max77620_rtc_softc *sc;
	struct clocktime ct;
	uint8_t buf[7];
	int rv;

	sc = device_get_softc(dev);

	LOCK(sc);
	rv = max77620_rtc_update(sc, true);
	if (rv != 0) {
		UNLOCK(sc);
		device_printf(sc->dev, "Failed to strobe RTC data\n");
		return (rv);
	}

	rv = max77620_rtc_read_buf(sc, MAX77620_RTC_SEC, buf, nitems(buf));
	UNLOCK(sc);
	if (rv != 0) {
		device_printf(sc->dev, "Failed to read RTC data\n");
		return (rv);
	}
	ct.nsec = 0;
	ct.sec  = bcd2bin(buf[0] & 0x7F);
	ct.min  = bcd2bin(buf[1] & 0x7F);
	ct.hour = bcd2bin(buf[2] & 0x3F);
	ct.dow  = ffs(buf[3] & 07);
	ct.mon  = bcd2bin(buf[4] & 0x1F);
	ct.year = bcd2bin(buf[5] & 0x7F) + MAX77620_RTC_START_YEAR;
	ct.day  = bcd2bin(buf[6] & 0x3F);

	return (clock_ct_to_ts(&ct, ts));
}

static int
max77620_rtc_settime(device_t dev, struct timespec *ts)
{
	struct max77620_rtc_softc *sc;
	struct clocktime ct;
	uint8_t buf[7];
	int rv;

	sc = device_get_softc(dev);
	clock_ts_to_ct(ts, &ct);

	if (ct.year < MAX77620_RTC_START_YEAR)
		return (EINVAL);

	buf[0] = bin2bcd(ct.sec);
	buf[1] = bin2bcd(ct.min);
	buf[2] = bin2bcd(ct.hour);
	buf[3] = 1 << ct.dow;
	buf[4] = bin2bcd(ct.mon);
	buf[5] = bin2bcd(ct.year - MAX77620_RTC_START_YEAR);
	buf[6] = bin2bcd(ct.day);

	LOCK(sc);
	rv = max77620_rtc_write_buf(sc, MAX77620_RTC_SEC, buf, nitems(buf));
	if (rv != 0) {
		UNLOCK(sc);
		device_printf(sc->dev, "Failed to write RTC data\n");
		return (rv);
	}
	rv = max77620_rtc_update(sc, false);
	UNLOCK(sc);
	if (rv != 0) {
		device_printf(sc->dev, "Failed to update RTC data\n");
		return (rv);
	}

	return (0);
}

static int
max77620_rtc_probe(device_t dev)
{
	struct iicbus_ivar *dinfo;

	dinfo = device_get_ivars(dev);
	if (dinfo == NULL)
		return (ENXIO);
	if (dinfo->addr != MAX77620_RTC_I2C_ADDR << 1)
		return (ENXIO);

	device_set_desc(dev, "MAX77620 RTC");
	return (BUS_PROBE_DEFAULT);
}

static int
max77620_rtc_attach(device_t dev)
{
	struct max77620_rtc_softc *sc;
	uint8_t reg;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->bus_addr = iicbus_get_addr(dev);

	LOCK_INIT(sc);

	reg = RTC_CONTROL_MODE_24 | RTC_CONTROL_BCD_EN;
	rv = max77620_rtc_modify(sc, MAX77620_RTC_CONTROLM, reg, reg);
	if (rv != 0) {
		device_printf(sc->dev, "Failed to configure RTC\n");
		goto fail;
	}

	rv = max77620_rtc_modify(sc, MAX77620_RTC_CONTROL, reg, reg);
	if (rv != 0) {
		device_printf(sc->dev, "Failed to configure RTC\n");
		goto fail;
	}
	rv = max77620_rtc_update(sc, false);
	if (rv != 0) {
		device_printf(sc->dev, "Failed to update RTC data\n");
		return (rv);
	}

	clock_register(sc->dev, 1000000);

	return (bus_generic_attach(dev));

fail:
	LOCK_DESTROY(sc);
	return (rv);
}

/*
 * The secondary address of MAX77620 (RTC function) is not in DTB,
 * add it manualy
 */
static int
max77620_rtc_detach(device_t dev)
{
	struct max77620_softc *sc;

	sc = device_get_softc(dev);
	LOCK_DESTROY(sc);

	return (bus_generic_detach(dev));
}

int
max77620_rtc_create(struct max77620_softc *sc, phandle_t node)
{
	device_t parent, child;
	struct iicbus_ivar *dinfo;

	parent = device_get_parent(sc->dev);
	child = BUS_ADD_CHILD(parent, 0, NULL, -1);
	if (child == 0)	{
		device_printf(sc->dev, "Cannot add MAX77620 RTC device.\n");
		return (ENXIO);
	}
	dinfo = device_get_ivars(child);
	if (dinfo == NULL)	{
		device_printf(sc->dev,
		    "Cannot set I2Caddress for MAX77620 RTC.\n");
		return (ENXIO);
	}
	dinfo->addr = MAX77620_RTC_I2C_ADDR << 1;
	return (0);
}

static device_method_t max77620_rtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		max77620_rtc_probe),
	DEVMETHOD(device_attach,	max77620_rtc_attach),
	DEVMETHOD(device_detach,	max77620_rtc_detach),

	/* RTC interface */
	DEVMETHOD(clock_gettime,	max77620_rtc_gettime),
	DEVMETHOD(clock_settime,	max77620_rtc_settime),

	DEVMETHOD_END
};

static devclass_t max77620_rtc_devclass;
static DEFINE_CLASS_0(rtc, max77620_rtc_driver, max77620_rtc_methods,
    sizeof(struct max77620_rtc_softc));
EARLY_DRIVER_MODULE(max77620rtc_, iicbus, max77620_rtc_driver,
    max77620_rtc_devclass, NULL, NULL, 74);
