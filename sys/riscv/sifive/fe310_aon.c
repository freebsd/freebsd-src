/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Axiado Corporation
 * All rights reserved.
 *
 * This software was developed in part by Nick O'Brien and Rishul Naik
 * for Axiado Corporation.
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
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sdt.h>
#include <sys/time.h>
#include <sys/timespec.h>
#include <sys/timex.h>
#include <sys/watchdog.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include "clock_if.h"

#define FEAON_AON_WDT_BASE		0x0
#define FEAON_AON_RTC_BASE		0x40
#define FEAON_AON_CLKCFG_BASE		0x70
#define FEAON_AON_BACKUP_BASE		0x80
#define FEAON_AON_PMU_BASE		0x100

/* Watchdog specific */
#define FEAON_WDT_CFG			0x0
#define FEAON_WDT_COUNT			0x8
#define FEAON_WDT_DOGS			0x10
#define FEAON_WDT_FEED			0x18
#define FEAON_WDT_KEY			0x1C
#define FEAON_WDT_CMP			0x20

#define FEAON_WDT_CFG_SCALE_MASK	0xF
#define FEAON_WDT_CFG_RST_EN		(1 << 8)
#define FEAON_WDT_CFG_ZERO_CMP		(1 << 9)
#define FEAON_WDT_CFG_EN_ALWAYS		(1 << 12)
#define FEAON_WDT_CFG_EN_CORE_AWAKE	(1 << 13)
#define FEAON_WDT_CFG_IP		(1 << 28)

#define FEAON_WDT_CMP_MASK		0xFFFF

#define FEAON_WDT_FEED_FOOD		0xD09F00D

#define FEAON_WDT_KEY_UNLOCK		0x51F15E

#define FEAON_WDT_TIMEBASE_FREQ		31250
#define FEAON_WDT_TIMEBASE_RATIO	(NANOSECOND / FEAON_WDT_TIMEBASE_FREQ)

/* Real-time clock specific */
#define FEAON_RTC_CFG			0x40
#define FEAON_RTC_LO			0x48
#define FEAON_RTC_HI			0x4C
#define FEAON_RTC_CMP			0x60

#define FEAON_RTC_CFG_SCALE_MASK	0xF
#define FEAON_RTC_CFG_EN		(1 << 12)
#define FEAON_RTC_CFG_IP		(1 << 28)

#define FEAON_RTC_HI_MASK		0xFFFF

#define FEAON_RTC_TIMEBASE_FREQ		31250LL

#define FEAON_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define FEAON_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define FEAON_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED)
#define FEAON_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED)

#define FEAON_READ_4(sc, reg)		bus_read_4(sc->reg_res, reg)
#define FEAON_WRITE_4(sc, reg, val)	bus_write_4(sc->reg_res, reg, val)

#define FEAON_WDT_WRITE_4(sc, reg, val) do {					\
		FEAON_WRITE_4(sc, (FEAON_WDT_KEY), (FEAON_WDT_KEY_UNLOCK));	\
		FEAON_WRITE_4(sc, reg, val);					\
	} while (0)

struct feaon_softc {
	device_t		dev;
	struct mtx		mtx;

	/* Resources */
	int			reg_rid;
	struct resource		*reg_res;

	/* WDT */
	eventhandler_tag	ev_tag;
};

static void
feaon_wdt_event(void *arg, unsigned int cmd, int *err)
{
	struct feaon_softc *sc;
	uint32_t scale, val;
	uint64_t time;

	sc = (struct feaon_softc *)arg;
	FEAON_LOCK(sc);

	/* First feed WDT */
	FEAON_WDT_WRITE_4(sc, FEAON_WDT_FEED, FEAON_WDT_FEED_FOOD);

	if ((cmd & WD_INTERVAL) == WD_TO_NEVER) {
		/* Disable WDT */
		val = FEAON_READ_4(sc, FEAON_WDT_CFG);
		val &= ~(FEAON_WDT_CFG_EN_ALWAYS | FEAON_WDT_CFG_EN_CORE_AWAKE);
		FEAON_WDT_WRITE_4(sc, FEAON_WDT_CFG, val);
		goto exit;
	}

	/* Calculate time in WDT frequency */
	time = 1LL << (cmd & WD_INTERVAL);
	time /= FEAON_WDT_TIMEBASE_RATIO;

	/* Fit time in CMP register with scale */
	scale = 0;
	while (time > FEAON_WDT_CMP_MASK) {
		time >>= 1;
		scale++;
	}

	if (time > FEAON_WDT_CMP_MASK || scale > FEAON_WDT_CFG_SCALE_MASK) {
		device_printf(sc->dev, "Time interval too large for WDT\n");
		*err = EINVAL;
		goto exit;
	}

	/* Program WDT */
	val = FEAON_READ_4(sc, FEAON_WDT_CFG);
	val &= ~FEAON_WDT_CFG_SCALE_MASK;
	val |= scale | FEAON_WDT_CFG_RST_EN | FEAON_WDT_CFG_EN_ALWAYS |
	    FEAON_WDT_CFG_ZERO_CMP;

	FEAON_WDT_WRITE_4(sc, FEAON_WDT_CMP, (uint32_t)time);
	FEAON_WDT_WRITE_4(sc, FEAON_WDT_CFG, val);

exit:
	FEAON_UNLOCK(sc);
}

static int
feaon_rtc_settime(device_t dev, struct timespec *ts)
{
	struct feaon_softc *sc;
	uint64_t time;
	uint32_t cfg;
	uint8_t scale;

	scale = 0;
	sc = device_get_softc(dev);

	FEAON_LOCK(sc);

	clock_dbgprint_ts(dev, CLOCK_DBG_WRITE, ts);

	time = ts->tv_sec * FEAON_RTC_TIMEBASE_FREQ;

	/* Find an appropriate scale */
	while (time >= 0xFFFFFFFFFFFFLL) {
		scale++;
		time >>= 1;
	}
	if (scale > FEAON_RTC_CFG_SCALE_MASK) {
		device_printf(sc->dev, "Time value too large for RTC\n");
		FEAON_UNLOCK(sc);
		return (1);
	}
	cfg = FEAON_READ_4(sc, FEAON_RTC_CFG) & ~FEAON_RTC_CFG_SCALE_MASK;
	cfg |= scale;

	FEAON_WRITE_4(sc, FEAON_RTC_CFG, cfg);
	FEAON_WRITE_4(sc, FEAON_RTC_LO, (uint32_t)time);
	FEAON_WRITE_4(sc, FEAON_RTC_HI, (time >> 32) & FEAON_RTC_HI_MASK);

	FEAON_UNLOCK(sc);

	return (0);
}

static int
feaon_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct feaon_softc *sc;
	uint64_t time;
	uint8_t scale;

	sc = device_get_softc(dev);
	FEAON_LOCK(sc);

	time = FEAON_READ_4(sc, FEAON_RTC_LO);
	time |= ((uint64_t)FEAON_READ_4(sc, FEAON_RTC_HI)) << 32;

	scale = FEAON_READ_4(sc, FEAON_RTC_CFG) & FEAON_RTC_CFG_SCALE_MASK;
	time <<= scale;

	ts->tv_sec = time / FEAON_RTC_TIMEBASE_FREQ;
	ts->tv_nsec = (time % FEAON_RTC_TIMEBASE_FREQ) *
	    (NANOSECOND / FEAON_RTC_TIMEBASE_FREQ);

	clock_dbgprint_ts(dev, CLOCK_DBG_READ, ts);

	FEAON_UNLOCK(sc);

	return (0);
}

static int
feaon_attach(device_t dev)
{
	struct feaon_softc *sc;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Mutex setup */
	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	/* Resource setup */
	sc->reg_rid = 0;
	if ((sc->reg_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->reg_rid, RF_ACTIVE)) == NULL) {
		device_printf(dev, "Error allocating memory resource.\n");
		err = ENXIO;
		goto error;
	}

	/* Enable RTC */
	clock_register(dev, 1000000); /* 1 sec resolution */
	FEAON_LOCK(sc);
	FEAON_WRITE_4(sc, FEAON_RTC_CFG, FEAON_RTC_CFG_EN);
	FEAON_UNLOCK(sc);

	/* Register WDT */
	sc->ev_tag = EVENTHANDLER_REGISTER(watchdog_list, feaon_wdt_event, sc, 0);

	return (0);

error:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->reg_rid, sc->reg_res);
	mtx_destroy(&sc->mtx);
	return (err);
}

static int
feaon_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "sifive,aon0"))
		return (ENXIO);

	device_set_desc(dev, "SiFive FE310 Always-On Controller");
	return (BUS_PROBE_DEFAULT);
}

static device_method_t feaon_methods[] = {
	DEVMETHOD(device_probe, feaon_probe),
	DEVMETHOD(device_attach, feaon_attach),

	/* RTC */
	DEVMETHOD(clock_gettime, feaon_rtc_gettime),
	DEVMETHOD(clock_settime, feaon_rtc_settime),

	DEVMETHOD_END
};

static driver_t feaon_driver = {
	"fe310aon",
	feaon_methods,
	sizeof(struct feaon_softc)
};

DRIVER_MODULE(fe310aon, simplebus, feaon_driver, 0, 0);
