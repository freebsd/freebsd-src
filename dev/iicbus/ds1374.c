/*-
 * Copyright (c) 2003-2012 Broadcom Corporation
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY BROADCOM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL BROADCOM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/rman.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"
#include "clock_if.h"

#define	DS1374_RTC_COUNTER	0	/* counter (bytes 0-3) */

struct ds1374_softc {
	uint32_t	sc_addr;
	device_t	sc_dev;
};

static int
ds1374_probe(device_t dev)
{
	device_set_desc(dev, "DS1374 RTC");
	return (0);
}

static int
ds1374_attach(device_t dev)
{
	struct ds1374_softc *sc = device_get_softc(dev);

	if(sc==NULL) {
		printf("ds1374_attach device_get_softc failed\n");
		return (0);
	}
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	clock_register(dev, 1000);
	return (0);
}

static int 
ds1374_settime(device_t dev, struct timespec *ts)
{
	/* NB: register pointer precedes actual data */
	uint8_t data[5] = { DS1374_RTC_COUNTER };
	struct ds1374_softc *sc = device_get_softc(dev);
	struct iic_msg msgs[1] = {
	     { sc->sc_addr, IIC_M_WR, 5, data },
	};

	data[1] = (ts->tv_sec >> 0) & 0xff;
	data[2] = (ts->tv_sec >> 8) & 0xff;
	data[3] = (ts->tv_sec >> 16) & 0xff;
	data[4] = (ts->tv_sec >> 24) & 0xff;

	return iicbus_transfer(dev, msgs, 1);
}

static int
ds1374_gettime(device_t dev, struct timespec *ts)
{
	struct ds1374_softc *sc = device_get_softc(dev);
	uint8_t addr[1] = { DS1374_RTC_COUNTER };
	uint8_t secs[4];
	struct iic_msg msgs[2] = {
	     { sc->sc_addr, IIC_M_WR, 1, addr },
	     { sc->sc_addr, IIC_M_RD, 4, secs },
	};
	int error;

	error = iicbus_transfer(dev, msgs, 2);
	if (error == 0) {
		/* counter has seconds since epoch */
		ts->tv_sec = (secs[3] << 24) | (secs[2] << 16)
			   | (secs[1] <<  8) | (secs[0] <<  0);
		ts->tv_nsec = 0;
	}
	return error;
}

static device_method_t ds1374_methods[] = {
	DEVMETHOD(device_probe,		ds1374_probe),
	DEVMETHOD(device_attach,	ds1374_attach),

	DEVMETHOD(clock_gettime,	ds1374_gettime),
	DEVMETHOD(clock_settime,	ds1374_settime),

	DEVMETHOD_END
};

static driver_t ds1374_driver = {
	"ds1374_rtc",
	ds1374_methods,
	sizeof(struct ds1374_softc),
};
static devclass_t ds1374_devclass;

DRIVER_MODULE(ds1374, iicbus, ds1374_driver, ds1374_devclass, 0, 0);
MODULE_VERSION(ds1374, 1);
MODULE_DEPEND(ds1374, iicbus, 1, 1, 1);
