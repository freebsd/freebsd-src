/*-
 * Copyright (c) 2006 Sam Leffler.  All rights reserved.
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
__FBSDID("$FreeBSD: src/sys/dev/iicbus/ds1672.c,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $");
/*
 * Dallas Semiconductor DS1672 RTC sitting on the I2C bus.
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

#include <dev/iicbus/iiconf.h>

#include "iicbus_if.h"
#include "clock_if.h"

#define	IIC_M_WR	0	/* write operation */

#define	DS1672_ADDR	0xd0	/* slave address */

#define	DS1672_COUNTER	0	/* counter (bytes 0-3) */
#define	DS1672_CTRL	4	/* control (1 byte) */
#define	DS1672_TRICKLE	5	/* trickle charger (1 byte) */

#define NANOSEC		1000000000

struct ds1672_softc {
	device_t		sc_dev;
};

static int
ds1672_probe(device_t dev)
{
	/* XXX really probe? */
	device_set_desc(dev, "Dallas Semiconductor DS1672 RTC");
	return (0);
}

static int
ds1672_attach(device_t dev)
{
	struct ds1672_softc *sc = device_get_softc(dev);

	sc->sc_dev = dev;

	clock_register(dev, 1000);
	return (0);
}

static int
ds1672_gettime(device_t dev, struct timespec *ts)
{
	uint8_t addr[1] = { DS1672_COUNTER };
	uint8_t secs[4];
	struct iic_msg msgs[2] = {
	     { DS1672_ADDR, IIC_M_WR, 1, addr },
	     { DS1672_ADDR, IIC_M_RD, 4, secs },
	};
	int error;

	error = iicbus_transfer(dev, msgs, 2);
	if (error == 0) {
		/* counter has seconds since epoch */
		ts->tv_sec = (secs[3] << 24) | (secs[2] << 16)
			   | (secs[1] <<  8) | (secs[0] <<  0);
		ts->tv_nsec = NANOSEC / 2;
	}
	return error;
}

static int
ds1672_settime(device_t dev, struct timespec *ts)
{
	/* NB: register pointer precedes actual data */
	uint8_t data[5] = { DS1672_COUNTER };
	struct iic_msg msgs[1] = {
	     { DS1672_ADDR, IIC_M_WR, 5, data },
	};

	data[1] = (ts->tv_sec >> 0) & 0xff;
	data[2] = (ts->tv_sec >> 8) & 0xff;
	data[3] = (ts->tv_sec >> 16) & 0xff;
	data[4] = (ts->tv_sec >> 24) & 0xff;

	return iicbus_transfer(dev, msgs, 1);
}

static device_method_t ds1672_methods[] = {
	DEVMETHOD(device_probe,		ds1672_probe),
	DEVMETHOD(device_attach,	ds1672_attach),

	DEVMETHOD(clock_gettime,	ds1672_gettime),
	DEVMETHOD(clock_settime,	ds1672_settime),

	{0, 0},
};

static driver_t ds1672_driver = {
	"ds1672",
	ds1672_methods,
	sizeof(struct ds1672_softc),
};
static devclass_t ds1672_devclass;

DRIVER_MODULE(ds1672, iicbus, ds1672_driver, ds1672_devclass, 0, 0);
MODULE_VERSION(ds1672, 1);
MODULE_DEPEND(ds1672, iicbus, 1, 1, 1);
