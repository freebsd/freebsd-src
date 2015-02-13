/*
 * Copyright (C) 2010 Andrew Turner
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/time.h>
#include <sys/clock.h>
#include <sys/resource.h>
#include <sys/systm.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <arm/samsung/s3c2xx0/s3c24x0reg.h>

#include "clock_if.h"

#define YEAR_BASE		2000

struct s3c2xx0_rtc_softc {
	struct resource *mem_res;
};

static int
s3c2xx0_rtc_probe(device_t dev)
{

	device_set_desc(dev, "Samsung Integrated RTC");
	return (0);
}

static int
s3c2xx0_rtc_attach(device_t dev)
{
	struct s3c2xx0_rtc_softc *sc;
	int error, rid;

	sc = device_get_softc(dev);
 	error = 0;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		error = ENOMEM;
		goto out;
	}

	bus_write_1(sc->mem_res, RTC_RTCCON, RTCCON_RTCEN);
	clock_register(dev, 1000000);

out:
	return (error);
}

static int
s3c2xx0_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct s3c2xx0_rtc_softc *sc;
	struct clocktime ct;

#define READ_TIME() do {						\
	ct.year = YEAR_BASE + FROMBCD(bus_read_1(sc->mem_res, RTC_BCDYEAR)); \
	ct.mon = FROMBCD(bus_read_1(sc->mem_res, RTC_BCDMON));		\
	ct.dow = FROMBCD(bus_read_1(sc->mem_res, RTC_BCDDAY));		\
	ct.day = FROMBCD(bus_read_1(sc->mem_res, RTC_BCDDATE));		\
	ct.hour = FROMBCD(bus_read_1(sc->mem_res, RTC_BCDHOUR));	\
	ct.min = FROMBCD(bus_read_1(sc->mem_res, RTC_BCDMIN));		\
	ct.sec = FROMBCD(bus_read_1(sc->mem_res, RTC_BCDSEC));		\
} while (0)

	sc = device_get_softc(dev);

	ct.nsec = 0;
	READ_TIME();
	/*
	 * Check if we could have read incorrect values
	 * as the values could have changed.
	 */
	if (ct.sec == 0) {
		READ_TIME();
	}

	ct.dow = -1;

#undef READ_TIME
	return (clock_ct_to_ts(&ct, ts));
}

static int
s3c2xx0_rtc_settime(device_t dev, struct timespec *ts)
{
	struct s3c2xx0_rtc_softc *sc;
	struct clocktime ct;

	sc = device_get_softc(dev);

	/* Resolution: 1 sec */
	if (ts->tv_nsec >= 500000000)
		ts->tv_sec++;
	ts->tv_nsec = 0;
	clock_ts_to_ct(ts, &ct);

	bus_write_1(sc->mem_res, RTC_BCDSEC, TOBCD(ct.sec));
	bus_write_1(sc->mem_res, RTC_BCDMIN, TOBCD(ct.min));
	bus_write_1(sc->mem_res, RTC_BCDHOUR, TOBCD(ct.hour));
	bus_write_1(sc->mem_res, RTC_BCDDATE, TOBCD(ct.day));
	bus_write_1(sc->mem_res, RTC_BCDDAY, TOBCD(ct.dow));
	bus_write_1(sc->mem_res, RTC_BCDMON, TOBCD(ct.mon));
	bus_write_1(sc->mem_res, RTC_BCDYEAR, TOBCD(ct.year - YEAR_BASE));

	return (0);
}

static device_method_t s3c2xx0_rtc_methods[] = {
	DEVMETHOD(device_probe,		s3c2xx0_rtc_probe),
	DEVMETHOD(device_attach,	s3c2xx0_rtc_attach),

	DEVMETHOD(clock_gettime,	s3c2xx0_rtc_gettime),
	DEVMETHOD(clock_settime,	s3c2xx0_rtc_settime),

	{ 0, 0 },
};

static driver_t s3c2xx0_rtc_driver = {
	"rtc",
	s3c2xx0_rtc_methods,
	sizeof(struct s3c2xx0_rtc_softc),
};
static devclass_t s3c2xx0_rtc_devclass;

DRIVER_MODULE(s3c2xx0_rtc, s3c24x0, s3c2xx0_rtc_driver, s3c2xx0_rtc_devclass,
    0, 0);

