/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Jessica Clarke <jrtc27@FreeBSD.org>
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

/*
 * RTC for the goldfish virtual hardware platform implemented in QEMU,
 * initially for Android but now also used for RISC-V's virt machine.
 *
 * https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/types.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include "clock_if.h"

#define	GOLDFISH_RTC_TIME_LOW	0x00
#define	GOLDFISH_RTC_TIME_HIGH	0x04

struct goldfish_rtc_softc {
	struct resource	*res;
	int		rid;
	struct mtx	mtx;
};

static int
goldfish_rtc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "google,goldfish-rtc")) {
		device_set_desc(dev, "Goldfish RTC");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
goldfish_rtc_attach(device_t dev)
{
	struct goldfish_rtc_softc *sc;

	sc = device_get_softc(dev);

	sc->rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
	    RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not allocate resource\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/*
	 * Register as a system realtime clock with 1 second resolution.
	 */
	clock_register_flags(dev, 1000000, CLOCKF_SETTIME_NO_ADJ);
	clock_schedule(dev, 1);

	return (0);
}

static int
goldfish_rtc_detach(device_t dev)
{
	struct goldfish_rtc_softc *sc;

	sc = device_get_softc(dev);

	clock_unregister(dev);
	mtx_destroy(&sc->mtx);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);

	return (0);
}

static int
goldfish_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct goldfish_rtc_softc *sc;
	uint64_t low, high, nsec;

	sc = device_get_softc(dev);

	/*
	 * Reading TIME_HIGH is defined in the documentation to give the high
	 * 32 bits corresponding to the last TIME_LOW read, so must be done in
	 * that order, but means we have atomicity guaranteed.
	 */
	mtx_lock(&sc->mtx);
	low = bus_read_4(sc->res, GOLDFISH_RTC_TIME_LOW);
	high = bus_read_4(sc->res, GOLDFISH_RTC_TIME_HIGH);
	mtx_unlock(&sc->mtx);

	nsec = (high << 32) | low;
	ts->tv_sec = nsec / 1000000000;
	ts->tv_nsec = nsec % 1000000000;

	return (0);
}

static int
goldfish_rtc_settime(device_t dev, struct timespec *ts)
{
	struct goldfish_rtc_softc *sc;
	uint64_t nsec;

	sc = device_get_softc(dev);

	/*
	 * We request a timespec with no resolution-adjustment.  That also
	 * disables utc adjustment, so apply that ourselves.
	 */
	ts->tv_sec -= utc_offset();
	nsec = (uint64_t)ts->tv_sec * 1000000000 + ts->tv_nsec;

	mtx_lock(&sc->mtx);
	bus_write_4(sc->res, GOLDFISH_RTC_TIME_HIGH, nsec >> 32);
	bus_write_4(sc->res, GOLDFISH_RTC_TIME_LOW, nsec);
	mtx_unlock(&sc->mtx);

	return (0);
}

static device_method_t goldfish_rtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		goldfish_rtc_probe),
	DEVMETHOD(device_attach,	goldfish_rtc_attach),
	DEVMETHOD(device_detach,	goldfish_rtc_detach),

	/* Clock interface */
	DEVMETHOD(clock_gettime,	goldfish_rtc_gettime),
	DEVMETHOD(clock_settime,	goldfish_rtc_settime),

	DEVMETHOD_END,
};

DEFINE_CLASS_0(goldfish_rtc, goldfish_rtc_driver, goldfish_rtc_methods,
    sizeof(struct goldfish_rtc_softc));

DRIVER_MODULE(goldfish_rtc, simplebus, goldfish_rtc_driver, NULL, NULL);
