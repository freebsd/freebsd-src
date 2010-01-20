/*-
 * Copyright (c) 2008 Stanislav Sedov <stas@FreeBSD.org>,
 *                    Rafal Jaworowski <raj@FreeBSD.org>,
 *                    Piotr Ziecik <kosmo@semihalf.com>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Dallas Semiconductor DS133X RTC sitting on the I2C bus.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/clock.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include "iicbus_if.h"
#include "clock_if.h"

#define DS133X_DEVNAME		"ds133x_rtc"

#define	DS133X_ADDR		0xd0	/* slave address */
#define	DS133X_DATE_REG		0x0
#define	DS133X_CTRL_REG		0x0e
#define	DS133X_OSCD_FLAG	0x80
#define	DS133X_OSF_FLAG		0x80

#define	DS133X_24H_FLAG		0x40	/* 24 hours mode. */
#define	DS133X_PM_FLAG		0x20	/* AM/PM bit. */
#define	DS133X_CENT_FLAG	0x80	/* Century selector. */
#define	DS133X_CENT_SHIFT	7

#define	DS1338_REG_CLOCK_HALT	0x00
#define	DS1338_REG_CONTROL	0x07
#define	DS1338_CLOCK_HALT	(1 << 7)
#define	DS1338_OSC_STOP		(1 << 5)

#define	DS1339_REG_CONTROL	0x0E
#define	DS1339_REG_STATUS	0x0F
#define	DS1339_OSC_STOP		(1 << 7)
#define	DS1339_ENABLE_OSC	(1 << 7)
#define	DS1339_BBSQI		(1 << 5)

#define	HALFSEC			500000000	/* 1/2 of second. */

#define MAX_IIC_DATA_SIZE	7

enum {
	DS1337,
	DS1338,
	DS1339,
};

struct ds133x_softc {
	int		sc_type;
	device_t	sc_dev;
};

static int
ds133x_read(device_t dev, uint8_t address, uint8_t *data, uint8_t size)
{
	struct iic_msg msg[] = {
	    { DS133X_ADDR, IIC_M_WR, 1,	&address },
	    { DS133X_ADDR, IIC_M_RD, size, data },
	};

	return (iicbus_transfer(dev, msg, 2));
}

static int
ds133x_write(device_t dev, uint8_t address, uint8_t *data, uint8_t size)
{
	uint8_t buffer[MAX_IIC_DATA_SIZE + 1];
	struct iic_msg msg[] = {
		{ DS133X_ADDR, IIC_M_WR, size + 1, buffer },
	};

	if (size > MAX_IIC_DATA_SIZE)
		return (ENOMEM);

	buffer[0] = address;
	memcpy(buffer + 1, data, size);

	return (iicbus_transfer(dev, msg, 1));
}

static int
ds133x_detect(device_t dev, int *sc_type)
{
	int error;
	uint8_t reg, orig;

	/*
	 * Check for DS1338. At address 0x0F this chip has RAM, however
	 * DS1337 and DS1339 have status register. Bits 6-2 in status
	 * register will be always read as 0.
	 */

	if ((error = ds133x_read(dev, DS1339_REG_STATUS, &reg, 1)))
		return (error);

	orig = reg;
	reg |= 0x7C;

	if ((error = ds133x_write(dev, DS1339_REG_STATUS, &reg, 1)))
		return (error);

	if ((error = ds133x_read(dev, DS1339_REG_STATUS, &reg, 1)))
		return (error);

	if ((reg & 0x7C) != 0) {
		/* This is DS1338 */

		if ((error = ds133x_write(dev, DS1339_REG_STATUS, &orig, 1)))
			return (error);

		*sc_type = DS1338;

		return (0);
	}

	/*
	 * Now Check for DS1337. Bit 5 in control register of this chip will be
	 * allways read as 0. In DS1339 changing of this bit is safe until
	 * chip is powered up.
	 */

	if ((error = ds133x_read(dev, DS1339_REG_CONTROL, &reg, 1)))
		return (error);

	orig = reg;
	reg |= DS1339_BBSQI;

	if ((error = ds133x_write(dev, DS1339_REG_CONTROL, &reg, 1)))
		return (error);

	if ((error = ds133x_read(dev, DS1339_REG_CONTROL, &reg, 1)))
		return (error);

	if ((reg & DS1339_BBSQI) != 0) {
		/* This is DS1339 */

		if ((error = ds133x_write(dev, DS1339_REG_CONTROL, &orig, 1)))
			return (error);

		*sc_type = DS1339;
		return (0);
	}

	/* This is DS1337 */
	*sc_type = DS1337;

	return (0);
}

static int
ds133x_init(device_t dev, uint8_t cs_reg, uint8_t cs_bit, uint8_t osf_reg,
    uint8_t osf_bit)
{
	int error;
	uint8_t reg;

	if ((error = ds133x_read(dev, cs_reg, &reg, 1)))
		return (error);

	if (reg & cs_bit) {	/* If clock is stopped - start it */
		reg &= ~cs_bit;
		if ((error = ds133x_write(dev, cs_reg, &reg, 1)))
			return (error);
	}

	if ((error = ds133x_read(dev, osf_reg, &reg, 1)))
		return (error);

	if (reg & osf_bit) {	/* Clear oscillator stop flag */
		device_printf(dev, "RTC oscillator was stopped. Check system"
		    " time and RTC battery.\n");
		reg &= ~osf_bit;
		if ((error = ds133x_write(dev, osf_reg, &reg, 1)))
			return (error);
	}

	return (0);
}


static void
ds133x_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, DS133X_DEVNAME, -1) == NULL)
		BUS_ADD_CHILD(parent, 0, DS133X_DEVNAME, -1);
}

static int
ds133x_probe(device_t dev)
{
	struct ds133x_softc *sc;
	int error;

	sc = device_get_softc(dev);

	if ((error = ds133x_detect(dev, &sc->sc_type)))
		return (error);

	switch (sc->sc_type) {
	case DS1337:
		device_set_desc(dev, "Dallas Semiconductor DS1337 RTC");
		break;
	case DS1338:
		device_set_desc(dev, "Dallas Semiconductor DS1338 RTC");
		break;
	case DS1339:
		device_set_desc(dev, "Dallas Semiconductor DS1339 RTC");
		break;
	default:
		break;
	}

	return (0);
}

static int
ds133x_attach(device_t dev)
{
	struct ds133x_softc *sc = device_get_softc(dev);

	sc->sc_dev = dev;

	if (sc->sc_type == DS1338)
		ds133x_init(dev, DS1338_REG_CLOCK_HALT, DS1338_CLOCK_HALT,
		    DS1338_REG_CONTROL, DS1338_OSC_STOP);
	else
		ds133x_init(dev, DS1339_REG_CONTROL, DS1339_ENABLE_OSC,
		    DS1339_REG_STATUS, DS1339_OSC_STOP);

	clock_register(dev, 1000000);

	return (0);
}

static uint8_t
ds133x_get_hours(uint8_t val)
{
	uint8_t ret;

	if (!(val & DS133X_24H_FLAG))
		ret = FROMBCD(val & 0x3f);
	else if (!(val & DS133X_PM_FLAG))
		ret = FROMBCD(val & 0x1f);
	else
		ret = FROMBCD(val & 0x1f) + 12;

	return (ret);
}

static int
ds133x_gettime(device_t dev, struct timespec *ts)
{
	struct ds133x_softc *sc = device_get_softc(dev);
	struct clocktime ct;
	uint8_t date[7];
	int error;

	error = ds133x_read(dev, DS133X_DATE_REG, date, 7);
	if (error == 0) {
		ct.nsec = 0;
		ct.sec = FROMBCD(date[0] & 0x7f);
		ct.min = FROMBCD(date[1] & 0x7f);
		ct.hour = ds133x_get_hours(date[2]);
		ct.dow = FROMBCD(date[3] & 0x07) - 1;
		ct.day = FROMBCD(date[4] & 0x3f);
		ct.mon = FROMBCD(date[5] & 0x1f);

		if (sc->sc_type == DS1338)
			ct.year = 2000 + FROMBCD(date[6]);
		else
			ct.year = 1900 + FROMBCD(date[6]) +
			    ((date[5] & DS133X_CENT_FLAG) >> DS133X_CENT_SHIFT) * 100;

		error = clock_ct_to_ts(&ct, ts);
	}

	return (error);
}

static int
ds133x_settime(device_t dev, struct timespec *ts)
{
	struct ds133x_softc *sc = device_get_softc(dev);
	struct clocktime ct;
	uint8_t date[7];

	clock_ts_to_ct(ts, &ct);

	date[0] = TOBCD(ct.nsec >= HALFSEC ? ct.sec + 1 : ct.sec) & 0x7f;
	date[1] = TOBCD(ct.min) & 0x7f;
	date[2] = TOBCD(ct.hour) & 0x3f;	/* We use 24-hours mode. */
	date[3] = TOBCD(ct.dow + 1) & 0x07;
	date[4] = TOBCD(ct.day) & 0x3f;
	date[5] = TOBCD(ct.mon) & 0x1f;
	if (sc->sc_type == DS1338)
		date[6] = TOBCD(ct.year - 2000);
	else if (ct.year >= 2000) {
		date[5] |= DS133X_CENT_FLAG;
		date[6] = TOBCD(ct.year - 2000);
	} else
		date[6] = TOBCD(ct.year - 1900);

	return (ds133x_write(dev, DS133X_DATE_REG, date, 7));
}

static device_method_t ds133x_methods[] = {
	DEVMETHOD(device_identify,	ds133x_identify),
	DEVMETHOD(device_probe,		ds133x_probe),
	DEVMETHOD(device_attach,	ds133x_attach),

	DEVMETHOD(clock_gettime,	ds133x_gettime),
	DEVMETHOD(clock_settime,	ds133x_settime),

	{0, 0},
};

static driver_t ds133x_driver = {
	DS133X_DEVNAME,
	ds133x_methods,
	sizeof(struct ds133x_softc),
};

static devclass_t ds133x_devclass;

DRIVER_MODULE(ds133x, iicbus, ds133x_driver, ds133x_devclass, 0, 0);
MODULE_VERSION(ds133x, 1);
MODULE_DEPEND(ds133x, iicbus, 1, 1, 1);
