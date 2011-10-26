/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <arm/at91/at91_rtcreg.h>

#include "clock_if.h"

struct at91_rtc_softc
{
	device_t dev;			/* Myself */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct mtx sc_mtx;		/* basically a perimeter lock */
};

static inline uint32_t
RD4(struct at91_rtc_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->mem_res, off);
}

static inline void
WR4(struct at91_rtc_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

#define AT91_RTC_LOCK(_sc)		mtx_lock_spin(&(_sc)->sc_mtx)
#define	AT91_RTC_UNLOCK(_sc)		mtx_unlock_spin(&(_sc)->sc_mtx)
#define AT91_RTC_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "rtc", MTX_SPIN)
#define AT91_RTC_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define AT91_RTC_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define AT91_RTC_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static devclass_t at91_rtc_devclass;

/* bus entry points */

static int at91_rtc_probe(device_t dev);
static int at91_rtc_attach(device_t dev);
static int at91_rtc_detach(device_t dev);
static int at91_rtc_intr(void *);

/* helper routines */
static int at91_rtc_activate(device_t dev);
static void at91_rtc_deactivate(device_t dev);

static int
at91_rtc_probe(device_t dev)
{
	device_set_desc(dev, "RTC");
	return (0);
}

static int
at91_rtc_attach(device_t dev)
{
	struct at91_rtc_softc *sc = device_get_softc(dev);
	int err;

	sc->dev = dev;
	err = at91_rtc_activate(dev);
	if (err)
		goto out;

	AT91_RTC_LOCK_INIT(sc);

	/*
	 * Activate the interrupt, but disable all interrupts in the hardware
	 */
	WR4(sc, RTC_IDR, 0xffffffff);
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC,
	    at91_rtc_intr, NULL, sc, &sc->intrhand);
	if (err) {
		AT91_RTC_LOCK_DESTROY(sc);
		goto out;
	}
	clock_register(dev, 1000000);
out:
	if (err)
		at91_rtc_deactivate(dev);
	return (err);
}

static int
at91_rtc_detach(device_t dev)
{
	return (EBUSY);	/* XXX */
}

static int
at91_rtc_activate(device_t dev)
{
	struct at91_rtc_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL)
		goto errout;
	return (0);
errout:
	at91_rtc_deactivate(dev);
	return (ENOMEM);
}

static void
at91_rtc_deactivate(device_t dev)
{
	struct at91_rtc_softc *sc;

	sc = device_get_softc(dev);
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = 0;
	bus_generic_detach(sc->dev);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = 0;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = 0;
	return;
}

static int
at91_rtc_intr(void *xsc)
{
	struct at91_rtc_softc *sc = xsc;
#if 0
	uint32_t status;

	/* Reading the status also clears the interrupt */
	status = RD4(sc, RTC_SR);
	if (status == 0)
		return;
	AT91_RTC_LOCK(sc);
	AT91_RTC_UNLOCK(sc);
#endif
	wakeup(sc);
	return (FILTER_HANDLED);
}

/*
 * Get the time of day clock and return it in ts.
 * Return 0 on success, an error number otherwise.
 */
static int
at91_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct clocktime ct;
	uint32_t timr, calr;
	struct at91_rtc_softc *sc;

	sc = device_get_softc(dev);
	timr = RD4(sc, RTC_TIMR);
	calr = RD4(sc, RTC_CALR);
	ct.nsec = 0;
	ct.sec = RTC_TIMR_SEC(timr);
	ct.min = RTC_TIMR_MIN(timr);
	ct.hour = RTC_TIMR_HR(timr);
	ct.year = RTC_CALR_CEN(calr) * 100 + RTC_CALR_YEAR(calr);
	ct.mon = RTC_CALR_MON(calr);
	ct.day = RTC_CALR_DAY(calr);
	ct.dow = -1;
	return clock_ct_to_ts(&ct, ts);
}

/*
 * Set the time of day clock based on the value of the struct timespec arg.
 * Return 0 on success, an error number otherwise.
 */
static int
at91_rtc_settime(device_t dev, struct timespec *ts)
{
	struct at91_rtc_softc *sc;
	struct clocktime ct;

	sc = device_get_softc(dev);
	clock_ts_to_ct(ts, &ct);
	WR4(sc, RTC_TIMR, RTC_TIMR_MK(ct.hour, ct.min, ct.sec));
	WR4(sc, RTC_CALR, RTC_CALR_MK(ct.year, ct.mon, ct.day, ct.dow));
	return (0);
}

static device_method_t at91_rtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at91_rtc_probe),
	DEVMETHOD(device_attach,	at91_rtc_attach),
	DEVMETHOD(device_detach,	at91_rtc_detach),

        /* clock interface */
        DEVMETHOD(clock_gettime,        at91_rtc_gettime),
        DEVMETHOD(clock_settime,        at91_rtc_settime),

	{ 0, 0 }
};

static driver_t at91_rtc_driver = {
	"at91_rtc",
	at91_rtc_methods,
	sizeof(struct at91_rtc_softc),
};

DRIVER_MODULE(at91_rtc, atmelarm, at91_rtc_driver, at91_rtc_devclass, 0, 0);
