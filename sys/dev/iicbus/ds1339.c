/*-
 * Copyright (c) 2008 Stanislav Sedov <stas@FreeBSD.org>,
 *                    Rafal Jaworowski <raj@FreeBSD.org>.
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
 * Dallas Semiconductor DS1339 RTC sitting on the I2C bus.
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

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include "iicbus_if.h"
#include "clock_if.h"

#define	DS1339_ADDR		0xd0	/* slave address */
#define	DS1339_DATE_REG		0x0
#define	DS1339_CTRL_REG		0x0e
#define	DS1339_OSCD_FLAG	0x80
#define	DS1339_OSF_FLAG		0x80

#define	DS1339_24H_FLAG		0x40	/* 24 hours mode. */
#define	DS1339_PM_FLAG		0x20	/* AM/PM bit. */
#define	DS1339_CENT_FLAG	0x80	/* Century selector. */
#define	DS1339_CENT_SHIFT	7

#define	HALFSEC	500000000	/* 1/2 of second. */

struct ds1339_softc {
	device_t		sc_dev;
};

static int
ds1339_probe(device_t dev)
{
	struct ds1339_softc *sc;
	int addr;

	sc = device_get_softc(dev);
	addr = iicbus_get_addr(dev);
	if (addr != DS1339_ADDR) {
		if (bootverbose)
			device_printf(dev, "fixed I2C slave address should "
			    "be 0x%.2x instead of 0x%.2x.\n", DS1339_ADDR,
			    addr);
	}
	device_set_desc(dev, "Dallas Semiconductor DS1339 RTC");
	return (0);
}

static int
ds1339_attach(device_t dev)
{
	struct ds1339_softc *sc = device_get_softc(dev);
	int error;

	sc->sc_dev = dev;

	uint8_t addr[1] = { DS1339_CTRL_REG };
	uint8_t rdata[2];
	uint8_t wrdata[3];
	struct iic_msg rmsgs[2] = {
	     { DS1339_ADDR, IIC_M_WR, 1, addr },
	     { DS1339_ADDR, IIC_M_RD, 2, rdata },
	};
	struct iic_msg wrmsgs[1] = {
	     { DS1339_ADDR, IIC_M_WR, 3, wrdata },
	};

	/* Read control and status registers. */
	error = iicbus_transfer(dev, rmsgs, 2);
	if (error != 0) {
		device_printf(dev, "could not read control registers.\n");
		return (ENXIO);
	}

	if ((rdata[0] & DS1339_OSCD_FLAG) != 0)
		rdata[0] &= ~DS1339_OSCD_FLAG;	/* Enable oscillator
						 * if disabled.
						 */
	if ((rdata[1] & DS1339_OSF_FLAG) != 0)
		rdata[1] &= ~DS1339_OSF_FLAG;	/* Clear oscillator stop flag */

	/* Write modified registers back. */
	wrdata[0] = DS1339_CTRL_REG;
	wrdata[1] = rdata[0];
	wrdata[2] = rdata[1];

	goto out;

	error = iicbus_transfer(dev, wrmsgs, 1);
	if (error != 0) {
		device_printf(dev, "could not write control registers.\n");
		return (ENXIO);
	}

out:

	clock_register(dev, 1000000);
	return (0);
}

static uint8_t
ds1339_get_hours(uint8_t val)
{
	uint8_t ret;

	if (!(val & DS1339_24H_FLAG))
		ret = FROMBCD(val & 0x3f);
	else if (!(val & DS1339_PM_FLAG))
		ret = FROMBCD(val & 0x1f);
	else
		ret = FROMBCD(val & 0x1f) + 12;

	return (ret);
}

static int
ds1339_gettime(device_t dev, struct timespec *ts)
{
	uint8_t addr[1] = { DS1339_DATE_REG };
	uint8_t date[7];
	struct iic_msg msgs[2] = {
	     { DS1339_ADDR, IIC_M_WR, 1, addr },
	     { DS1339_ADDR, IIC_M_RD, 7, date },
	};
	struct clocktime ct;
	int error;

	error = iicbus_transfer(dev, msgs, 2);
	if (error == 0) {
		ct.nsec = 0;
		ct.sec = FROMBCD(date[0] & 0x7f);
		ct.min = FROMBCD(date[1] & 0x7f);
		ct.hour = ds1339_get_hours(date[2]);
		ct.dow = FROMBCD(date[3] & 0x07) - 1;
		ct.day = FROMBCD(date[4] & 0x3f);
		ct.mon = FROMBCD(date[5] & 0x1f);
		ct.year = 1900 + FROMBCD(date[6]) +
		    ((date[5] & DS1339_CENT_FLAG) >> DS1339_CENT_SHIFT) * 100;

		error = clock_ct_to_ts(&ct, ts);
	}

	return error;
}

static int
ds1339_settime(device_t dev, struct timespec *ts)
{
	uint8_t data[8] = { DS1339_DATE_REG };	/* Register address. */
	struct iic_msg msgs[1] = {
	     { DS1339_ADDR, IIC_M_WR, 8, data },
	};
	struct clocktime ct;

	clock_ts_to_ct(ts, &ct);

	data[1] = TOBCD(ct.nsec >= HALFSEC ? ct.sec + 1 : ct.sec) & 0x7f;
	data[2] = TOBCD(ct.min) & 0x7f;
	data[3] = TOBCD(ct.hour) & 0x3f;	/* We use 24-hours mode. */
	data[4] = TOBCD(ct.dow + 1) & 0x07;
	data[5] = TOBCD(ct.day) & 0x3f;
	data[6] = TOBCD(ct.mon) & 0x1f;
	if (ct.year >= 2000) {
		data[6] |= DS1339_CENT_FLAG;
		data[7] = TOBCD(ct.year - 2000);
	} else
		data[7] = TOBCD(ct.year - 1900);

	return iicbus_transfer(dev, msgs, 1);
}

static device_method_t ds1339_methods[] = {
	DEVMETHOD(device_probe,		ds1339_probe),
	DEVMETHOD(device_attach,	ds1339_attach),

	DEVMETHOD(clock_gettime,	ds1339_gettime),
	DEVMETHOD(clock_settime,	ds1339_settime),

	{0, 0},
};

static driver_t ds1339_driver = {
	"ds1339",
	ds1339_methods,
	sizeof(struct ds1339_softc),
};
static devclass_t ds1339_devclass;

DRIVER_MODULE(ds1339, iicbus, ds1339_driver, ds1339_devclass, 0, 0);
MODULE_VERSION(ds1339, 1);
MODULE_DEPEND(ds1339, iicbus, 1, 1, 1);
