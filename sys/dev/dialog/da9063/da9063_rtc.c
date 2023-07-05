/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Jessica Clarke <jrtc27@FreeBSD.org>
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

/* Dialog Semiconductor DA9063 RTC */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/dialog/da9063/da9063reg.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clock_if.h"
#include "da9063_if.h"

#define	DA9063_RTC_BASE_YEAR	2000

struct da9063_rtc_softc {
	device_t	dev;
	device_t	parent;
	struct mtx	mtx;
};

#define	DA9063_RTC_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define	DA9063_RTC_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	DA9063_RTC_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->mtx, MA_OWNED);
#define	DA9063_RTC_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED);

static struct ofw_compat_data compat_data[] = {
	{ "dlg,da9063-rtc",	1 },
	{ NULL,			0 }
};

static int
da9063_rtc_read_ct(struct da9063_rtc_softc *sc, struct clocktime *ct)
{
	uint8_t sec, min, hour, day, mon, year;
	int error;

	DA9063_RTC_ASSERT_LOCKED(sc)

	error = DA9063_READ(sc->parent, DA9063_COUNT_S, &sec);
	if (error != 0)
		return (error);
	if ((sec & DA9063_COUNT_S_RTC_READ) == 0)
		return (EAGAIN);

	error = DA9063_READ(sc->parent, DA9063_COUNT_MI, &min);
	if (error != 0)
		return (error);

	error = DA9063_READ(sc->parent, DA9063_COUNT_H, &hour);
	if (error != 0)
		return (error);

	error = DA9063_READ(sc->parent, DA9063_COUNT_D, &day);
	if (error != 0)
		return (error);

	error = DA9063_READ(sc->parent, DA9063_COUNT_MO, &mon);
	if (error != 0)
		return (error);

	error = DA9063_READ(sc->parent, DA9063_COUNT_Y, &year);
	if (error != 0)
		return (error);

	ct->nsec = 0;
	ct->dow = -1;
	ct->sec = sec & DA9063_COUNT_S_COUNT_SEC_MASK;
	ct->min = min & DA9063_COUNT_MI_COUNT_MIN_MASK;
	ct->hour = hour & DA9063_COUNT_H_COUNT_HOUR_MASK;
	ct->day = day & DA9063_COUNT_D_COUNT_DAY_MASK;
	ct->mon = mon & DA9063_COUNT_MO_COUNT_MONTH_MASK;
	ct->year = (year & DA9063_COUNT_Y_COUNT_YEAR_MASK) +
	    DA9063_RTC_BASE_YEAR;

	return (0);
}

static int
da9063_rtc_write_ct(struct da9063_rtc_softc *sc, struct clocktime *ct)
{
	int error;

	DA9063_RTC_ASSERT_LOCKED(sc)

	error = DA9063_WRITE(sc->parent, DA9063_COUNT_S, ct->sec);
	if (error != 0)
		return (error);

	error = DA9063_WRITE(sc->parent, DA9063_COUNT_MI, ct->min);
	if (error != 0)
		return (error);

	error = DA9063_WRITE(sc->parent, DA9063_COUNT_H, ct->hour);
	if (error != 0)
		return (error);

	error = DA9063_WRITE(sc->parent, DA9063_COUNT_D, ct->day);
	if (error != 0)
		return (error);

	error = DA9063_WRITE(sc->parent, DA9063_COUNT_MO, ct->mon);
	if (error != 0)
		return (error);

	error = DA9063_WRITE(sc->parent, DA9063_COUNT_Y,
	    (ct->year - DA9063_RTC_BASE_YEAR) &
	    DA9063_COUNT_Y_COUNT_YEAR_MASK);
	if (error != 0)
		return (error);

	return (0);
}

static int
da9063_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct da9063_rtc_softc *sc;
	struct clocktime ct, oldct;
	int error;

	sc = device_get_softc(dev);

	DA9063_RTC_LOCK(sc);

	error = da9063_rtc_read_ct(sc, &ct);
	if (error != 0)
		goto error;

	/*
	 * Reading seconds only latches the other registers for "approx 0.5s",
	 * which should almost always be sufficient but is not guaranteed to
	 * be, so re-read to get a consistent set of values.
	 */
	do {
		oldct = ct;
		error = da9063_rtc_read_ct(sc, &ct);
		if (error != 0)
			goto error;
	} while (ct.min != oldct.min || ct.hour != oldct.hour ||
	    ct.day != oldct.day || ct.mon != oldct.mon ||
	    ct.year != oldct.year);

	DA9063_RTC_UNLOCK(sc);

	error = clock_ct_to_ts(&ct, ts);
	if (error != 0)
		return (error);

	return (0);

error:
	DA9063_RTC_UNLOCK(sc);
	return (error);
}

static int
da9063_rtc_settime(device_t dev, struct timespec *ts)
{
	struct da9063_rtc_softc *sc;
	struct clocktime ct;
	int error;

	sc = device_get_softc(dev);

	/*
	 * We request a timespec with no resolution-adjustment.  That also
	 * disables utc adjustment, so apply that ourselves.
	 */
	ts->tv_sec -= utc_offset();
	clock_ts_to_ct(ts, &ct);

	DA9063_RTC_LOCK(sc);
	error = da9063_rtc_write_ct(sc, &ct);
	DA9063_RTC_UNLOCK(sc);

	return (error);
}

static int
da9063_rtc_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Dialog DA9063 RTC");

	return (BUS_PROBE_DEFAULT);
}

static int
da9063_rtc_attach(device_t dev)
{
	struct da9063_rtc_softc *sc;
	int error;

	sc = device_get_softc(dev);

	sc->dev = dev;
	sc->parent = device_get_parent(dev);

	/* Power on RTC and 32 kHz oscillator */
	error = DA9063_MODIFY(sc->parent, DA9063_CONTROL_E, 0,
	    DA9063_CONTROL_E_RTC_EN);
	if (error != 0)
		return (error);

	/* Connect 32 kHz oscillator */
	error = DA9063_MODIFY(sc->parent, DA9063_EN_32K, 0,
	    DA9063_EN_32K_CRYSTAL);
	if (error != 0)
		return (error);

	/* Disable alarms */
	error = DA9063_MODIFY(sc->parent, DA9063_ALARM_Y,
	    DA9063_ALARM_Y_ALARM_ON | DA9063_ALARM_Y_TICK_ON, 0);
	if (error != 0)
		return (error);

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	/*
	 * Register as a system realtime clock with 1 second resolution.
	 */
	clock_register_flags(dev, 1000000, CLOCKF_SETTIME_NO_ADJ);
	clock_schedule(dev, 1);

	return (0);
}

static int
da9063_rtc_detach(device_t dev)
{
	struct da9063_rtc_softc *sc;

	sc = device_get_softc(dev);

	clock_unregister(dev);
	mtx_destroy(&sc->mtx);

	return (0);
}

static device_method_t da9063_rtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		da9063_rtc_probe),
	DEVMETHOD(device_attach,	da9063_rtc_attach),
	DEVMETHOD(device_detach,	da9063_rtc_detach),

	/* Clock interface */
	DEVMETHOD(clock_gettime,	da9063_rtc_gettime),
	DEVMETHOD(clock_settime,	da9063_rtc_settime),

	DEVMETHOD_END,
};

DEFINE_CLASS_0(da9063_rtc, da9063_rtc_driver, da9063_rtc_methods,
    sizeof(struct da9063_rtc_softc));

DRIVER_MODULE(da9063_rtc, da9063_pmic, da9063_rtc_driver, NULL, NULL);
