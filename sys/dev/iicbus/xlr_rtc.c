/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * RMI_BSD */

#include <sys/cdefs.h>
/*
 *  RTC chip sitting on the I2C bus.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/rman.h>

#include <mips/include/bus.h>
#include <mips/include/cpu.h>
#include <mips/include/cpufunc.h>
#include <mips/include/frame.h>
#include <mips/include/resource.h>

#include <dev/iicbus/iiconf.h>

#include "iicbus_if.h"
#include "clock_if.h"

#define	IIC_M_WR	0	/* write operation */
#define	XLR_SLAVE_ADDR	(0xd0 )	/* slave address */
#define	XLR_RTC_COUNTER	0	/* counter (bytes 0-3) */
#define NANOSEC		1000000000

struct xlr_rtc_softc {
	device_t		sc_dev;
};

static int
xlr_rtc_probe(device_t dev)
{
	device_set_desc(dev, "RTC on XLR board");
	return (0);
}

static int
xlr_rtc_attach(device_t dev)
{
	struct xlr_rtc_softc *sc = device_get_softc(dev);

	sc->sc_dev = dev;

	clock_register(dev, 1000);
	return (0);
}

static int
xlr_rtc_gettime(device_t dev, struct timespec *ts)
{
	uint8_t addr[1] = { XLR_RTC_COUNTER };
	uint8_t secs[4];
	struct iic_msg msgs[2] = {
	     { XLR_SLAVE_ADDR, IIC_M_WR, 1, addr },
	     { XLR_SLAVE_ADDR, IIC_M_RD, 4, secs },
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
xlr_rtc_settime(device_t dev, struct timespec *ts)
{
	/* NB: register pointer precedes actual data */
	uint8_t data[5] = { XLR_RTC_COUNTER };
	struct iic_msg msgs[1] = {
	     { XLR_SLAVE_ADDR, IIC_M_WR, 5, data },
	};

	data[1] = (ts->tv_sec >> 0) & 0xff;
	data[2] = (ts->tv_sec >> 8) & 0xff;
	data[3] = (ts->tv_sec >> 16) & 0xff;
	data[4] = (ts->tv_sec >> 24) & 0xff;

	return iicbus_transfer(dev, msgs, 1);
}

static device_method_t xlr_rtc_methods[] = {
	DEVMETHOD(device_probe,		xlr_rtc_probe),
	DEVMETHOD(device_attach,	xlr_rtc_attach),

	DEVMETHOD(clock_gettime,	xlr_rtc_gettime),
	DEVMETHOD(clock_settime,	xlr_rtc_settime),

	{0, 0},
};

static driver_t xlr_rtc_driver = {
	"xlr_rtc",
	xlr_rtc_methods,
	sizeof(struct xlr_rtc_softc),
};
static devclass_t xlr_rtc_devclass;

DRIVER_MODULE(xlr_rtc, iicbus, xlr_rtc_driver, xlr_rtc_devclass, 0, 0);
MODULE_VERSION(xlr_rtc, 1);
MODULE_DEPEND(xlr_rtc, iicbus, 1, 1, 1);
