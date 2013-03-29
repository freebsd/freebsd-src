/*-
 * Copyright (c) 2012 Marius Strobl <marius@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for NXP PCF8563 real-time clock/calendar
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/pcf8563reg.h>

#include "clock_if.h"
#include "iicbus_if.h"

#define	PCF8563_NCLOCKREGS	(PCF8563_R_YEAR - PCF8563_R_CS1 + 1)

struct pcf8563_softc {
	uint32_t	sc_flags;
#define	PCF8563_CPOL	(1 << 0)	/* PCF8563_R_MONTH_C means 19xx */
	uint16_t	sc_addr;	/* PCF8563 slave address */
	uint16_t	sc_year0;	/* TOD clock year 0 */
};

static device_attach_t pcf8563_attach;
static device_probe_t pcf8563_probe;
static clock_gettime_t pcf8563_gettime;
static clock_settime_t pcf8563_settime;

static int
pcf8563_probe(device_t dev)
{

	device_set_desc(dev, "NXP PCF8563 RTC");
	return (BUS_PROBE_NOWILDCARD);
}

static int
pcf8563_attach(device_t dev)
{
	uint8_t reg = PCF8563_R_SECOND, val;
	struct iic_msg msgs[] = {
		{ 0, IIC_M_WR, sizeof(reg), &reg },
		{ 0, IIC_M_RD, sizeof(val), &val }
	};
	struct pcf8563_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->sc_addr = iicbus_get_addr(dev);
	if (sc->sc_addr == 0)
		sc->sc_addr = PCF8563_ADDR;

	/*
	 * NB: PCF8563_R_SECOND_VL doesn't automatically clear when VDD
	 * rises above Vlow again and needs to be cleared manually.
	 * However, apparently this needs all of the time registers to be
	 * set, i.e. pcf8563_settime(), and not just PCF8563_R_SECOND in
	 * order for PCF8563_R_SECOND_VL to stick.  Thus, we just issue a
	 * warning here rather than failing with ENXIO in case it is set.
	 * Note that pcf8563_settime() will also clear PCF8563_R_SECOND_VL
	 * as a side-effect.
	 */
	msgs[0].slave = msgs[1].slave = sc->sc_addr;
	error = iicbus_transfer(device_get_parent(dev), msgs, sizeof(msgs) /
	    sizeof(*msgs));
	if (error != 0) {
		device_printf(dev, "%s: cannot read RTC\n", __func__);
		return (error);
	}
	if ((val & PCF8563_R_SECOND_VL) != 0)
		device_printf(dev, "%s: battery low\n", __func__);

	sc->sc_year0 = 1900;
	clock_register(dev, 1000000);   /* 1 second resolution */
	return (0);
}

static int
pcf8563_gettime(device_t dev, struct timespec *ts)
{
	struct clocktime ct;
	uint8_t reg = PCF8563_R_SECOND, val[PCF8563_NCLOCKREGS];
	struct iic_msg msgs[] = {
		{ 0, IIC_M_WR, sizeof(reg), &reg },
		{ 0, IIC_M_RD, PCF8563_NCLOCKREGS, &val[PCF8563_R_SECOND] }
	};
	struct pcf8563_softc *sc;
	int error;

	sc = device_get_softc(dev);
	msgs[0].slave = msgs[1].slave = sc->sc_addr;
	error = iicbus_transfer(device_get_parent(dev), msgs, sizeof(msgs) /
	    sizeof(*msgs));
	if (error != 0) {
		device_printf(dev, "%s: cannot read RTC\n", __func__);
		return (error);
	}

	ct.nsec = 0;
	ct.sec = FROMBCD(val[PCF8563_R_SECOND] & PCF8563_M_SECOND);
	ct.min = FROMBCD(val[PCF8563_R_MINUTE] & PCF8563_M_MINUTE);
	ct.hour = FROMBCD(val[PCF8563_R_HOUR] & PCF8563_M_HOUR);
	ct.day = FROMBCD(val[PCF8563_R_DAY] & PCF8563_M_DAY);
	ct.dow = val[PCF8563_R_WEEKDAY] & PCF8563_M_WEEKDAY;
	ct.mon = FROMBCD(val[PCF8563_R_MONTH] & PCF8563_M_MONTH);
	ct.year = FROMBCD(val[PCF8563_R_YEAR] & PCF8563_M_YEAR);
	ct.year += sc->sc_year0;
	if (ct.year < POSIX_BASE_YEAR)
		ct.year += 100;	/* assume [1970, 2069] */
	if ((val[PCF8563_R_MONTH] & PCF8563_R_MONTH_C) != 0) {
		if (ct.year >= 100 + sc->sc_year0)
			sc->sc_flags |= PCF8563_CPOL;
	} else if (ct.year < 100 + sc->sc_year0)
			sc->sc_flags |= PCF8563_CPOL;
	return (clock_ct_to_ts(&ct, ts));
}

static int
pcf8563_settime(device_t dev, struct timespec *ts)
{
	struct clocktime ct;
	uint8_t val[PCF8563_NCLOCKREGS];
	struct iic_msg msgs[] = {
		{ 0, IIC_M_WR, PCF8563_NCLOCKREGS - 1, &val[PCF8563_R_CS2] }
	};
	struct pcf8563_softc *sc;
	int error;

	sc = device_get_softc(dev);
	val[PCF8563_R_CS2] = PCF8563_R_SECOND;	/* abuse */
	/* Accuracy is only one second. */
	if (ts->tv_nsec >= 500000000)
		ts->tv_sec++;
	ts->tv_nsec = 0;
	clock_ts_to_ct(ts, &ct);
	val[PCF8563_R_SECOND] = TOBCD(ct.sec);
	val[PCF8563_R_MINUTE] = TOBCD(ct.min);
	val[PCF8563_R_HOUR] = TOBCD(ct.hour);
	val[PCF8563_R_DAY] = TOBCD(ct.day);
	val[PCF8563_R_WEEKDAY] = ct.dow;
	val[PCF8563_R_MONTH] = TOBCD(ct.mon);
	val[PCF8563_R_YEAR] = TOBCD(ct.year % 100);
	if ((sc->sc_flags & PCF8563_CPOL) != 0) {
		if (ct.year >= 100 + sc->sc_year0)
			val[PCF8563_R_MONTH] |= PCF8563_R_MONTH_C;
	} else if (ct.year < 100 + sc->sc_year0)
			val[PCF8563_R_MONTH] |= PCF8563_R_MONTH_C;

	msgs[0].slave = sc->sc_addr;
	error = iicbus_transfer(device_get_parent(dev), msgs, sizeof(msgs) /
	    sizeof(*msgs));
	if (error != 0)
		device_printf(dev, "%s: cannot write RTC\n", __func__);
	return (error);
}

static device_method_t pcf8563_methods[] = {
	DEVMETHOD(device_probe,		pcf8563_probe),
	DEVMETHOD(device_attach,	pcf8563_attach),

	DEVMETHOD(clock_gettime,	pcf8563_gettime),
	DEVMETHOD(clock_settime,	pcf8563_settime),

	DEVMETHOD_END
};

static driver_t pcf8563_driver = {
	"pcf8563_rtc",
	pcf8563_methods,
	sizeof(struct pcf8563_softc),
};

static devclass_t pcf8563_devclass;

DRIVER_MODULE(pcf8563, iicbus, pcf8563_driver, pcf8563_devclass, NULL, NULL);
MODULE_VERSION(pcf8563, 1);
MODULE_DEPEND(pcf8563, iicbus, 1, 1, 1);
