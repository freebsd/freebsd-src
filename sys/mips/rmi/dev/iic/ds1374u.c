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
__FBSDID("$FreeBSD$");
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
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"
#include "clock_if.h"

#define	DS1374_RTC_COUNTER	0	/* counter (bytes 0-3) */

struct ds1374u_softc {
	uint32_t	sc_addr;
	device_t	sc_dev;
};

static int
ds1374u_probe(device_t dev)
{
	device_set_desc(dev, "DS1374U-33 RTC");
	return (0);
}

static int
ds1374u_attach(device_t dev)
{
	struct ds1374u_softc *sc = device_get_softc(dev);

	if(sc==NULL) {
		printf("ds1374u_attach device_get_softc failed\n");
		return (0);
	}
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	clock_register(dev, 1000);
	return (0);
}


static int
ds1374u_write(device_t dev, int reg, uint8_t val) 
{
	uint8_t data[2];
	struct ds1374u_softc *sc = device_get_softc(dev);
	struct iic_msg msgs[1] = {
	     { sc->sc_addr, IIC_M_WR, 2, data },
	};

	data[0] = reg;
	data[1] = val;
	if (iicbus_transfer(dev, msgs, 1) == 0) 
		return (0);
	else
		return (-1);
}

static int 
ds1374u_settime(device_t dev, struct timespec *ts)
{
	int i; 
	int temp = 0; 

	for (i = 0; i < 4; i++) {
		temp = (ts->tv_sec >> (8*i)) & 0xff; 
		if (ds1374u_write(dev, DS1374_RTC_COUNTER+i, temp)!=0)
			return (-1);
	}
	return 0;
}

static int
ds1374u_gettime(device_t dev, struct timespec *ts)
{
	struct ds1374u_softc *sc = device_get_softc(dev);
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
	return 0;
}

static device_method_t ds1374u_methods[] = {
	DEVMETHOD(device_probe,		ds1374u_probe),
	DEVMETHOD(device_attach,	ds1374u_attach),

	DEVMETHOD(clock_gettime,	ds1374u_gettime),
	DEVMETHOD(clock_settime,	ds1374u_settime),

	{0, 0},
};

static driver_t ds1374u_driver = {
	"ds1374u",
	ds1374u_methods,
	sizeof(struct ds1374u_softc),
};
static devclass_t ds1374u_devclass;

DRIVER_MODULE(ds1374u, iicbus, ds1374u_driver, ds1374u_devclass, 0, 0);
MODULE_VERSION(ds1374u, 1);
MODULE_DEPEND(ds1374u, iicbus, 1, 1, 1);
