/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 * Copyright (c) 2012 Ian Lepore.  All rights reserved.
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

/*
 * Driver for the at91 on-chip realtime clock.
 *
 * This driver does not currently support alarms, just date and time.
 *
 * The RTC on the AT91RM9200 resets when the core rests, so it is useless as a
 * source of time (except when the CPU clock is powered down to save power,
 * which we don't currently do).  On AT91SAM9 chips, the RTC survives chip
 * reset, and there's provisions for it to keep time via battery backup if the
 * system loses power.  On those systems, we use it as a RTC.  We tell the two
 * apart because the century field is 19 on AT91RM9200 on reset, or on AT91SAM9
 * chips that haven't had their time properly set.
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
#include <machine/cpu.h>

#include <arm/at91/at91_rtcreg.h>

#include "clock_if.h"

/*
 * The driver has all the infrastructure to use interrupts but doesn't actually
 * have any need to do so right now.  There's a non-zero cost for installing the
 * handler because the RTC shares the system interrupt (IRQ 1), and thus will
 * get called a lot for no reason at all.
 */
#define	AT91_RTC_USE_INTERRUPTS_NOT

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

/* helper routines */
static int at91_rtc_activate(device_t dev);
static void at91_rtc_deactivate(device_t dev);

#ifdef AT91_RTC_USE_INTERRUPTS
static int
at91_rtc_intr(void *xsc)
{
	struct at91_rtc_softc *sc;
	uint32_t status;

	sc = xsc;
	/* Must clear the status bits after reading them to re-arm. */
	status = RD4(sc, RTC_SR);
	WR4(sc, RTC_SCCR, status);
	if (status == 0)
		return;
	AT91_RTC_LOCK(sc);
        /* Do something here */
	AT91_RTC_UNLOCK(sc);
	wakeup(sc);
	return (FILTER_HANDLED);
}
#endif

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
	 * Disable all interrupts in the hardware.
	 * Clear all bits in the status register.
	 * Set 24-hour-clock mode.
	 */
	WR4(sc, RTC_IDR, 0xffffffff);
	WR4(sc, RTC_SCCR, 0x1f);
	WR4(sc, RTC_MR, 0);

#ifdef AT91_RTC_USE_INTERRUPTS
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC,
	    at91_rtc_intr, NULL, sc, &sc->intrhand);
	if (err) {
		AT91_RTC_LOCK_DESTROY(sc);
		goto out;
	}
#endif	

	/*
	 * Read the calendar register.  If the century is 19 then the clock has
	 * never been set.  Try to store an invalid value into the register,
	 * which will turn on the error bit in RTC_VER, and our getclock code
	 * knows to return EINVAL if any error bits are on.
	 */
	if (RTC_CALR_CEN(RD4(sc, RTC_CALR)) == 19)
		WR4(sc, RTC_CALR, 0);

	/*
	 * Register as a time of day clock with 1-second resolution.
	 */
	clock_register(dev, 1000000);
out:
	if (err)
		at91_rtc_deactivate(dev);
	return (err);
}

/*
 * Cannot support detach, since there's no clock_unregister function.
 */
static int
at91_rtc_detach(device_t dev)
{
	return (EBUSY);
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
#ifdef AT91_RTC_USE_INTERRUPTS
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL)
		goto errout;
#endif	
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
#ifdef AT91_RTC_USE_INTERRUPTS
	WR4(sc, RTC_IDR, 0xffffffff);
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = 0;
#endif
	bus_generic_detach(sc->dev);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = 0;
#ifdef AT91_RTC_USE_INTERRUPTS
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = 0;
#endif	
	return;
}

/*
 * Get the time of day clock and return it in ts.
 * Return 0 on success, an error number otherwise.
 */
static int
at91_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct clocktime ct;
	uint32_t calr, calr2, timr, timr2;
	struct at91_rtc_softc *sc;

	sc = device_get_softc(dev);

	/* If the error bits are set we can't return useful values. */

	if (RD4(sc, RTC_VER) & (RTC_VER_NVTIM | RTC_VER_NVCAL))
		return EINVAL;

	/*
	 * The RTC hardware can update registers while the CPU is reading them.
	 * The manual advises reading until you obtain the same values twice.
	 * Interleaving the reads (rather than timr, timr2, calr, calr2 order)
	 * also ensures we don't miss a midnight rollover/carry between reads.
	 */
	do {
		timr = RD4(sc, RTC_TIMR);
		calr = RD4(sc, RTC_CALR);
		timr2 = RD4(sc, RTC_TIMR);
		calr2 = RD4(sc, RTC_CALR);
	} while (timr != timr2 || calr != calr2);

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
	int rv;

	sc = device_get_softc(dev);
	clock_ts_to_ct(ts, &ct);

	/*
	 * Can't set the clock unless a second has elapsed since we last did so.
	 */
	while ((RD4(sc, RTC_SR) & RTC_SR_SECEV) == 0)
		cpu_spinwait();

	/*
	 * Stop the clocks for an update; wait until hardware is ready.
	 * Clear the update-ready status after it gets asserted (the manual says
	 * to do this before updating the value registers).
	 */
	WR4(sc, RTC_CR, RTC_CR_UPDCAL | RTC_CR_UPDTIM);
	while ((RD4(sc, RTC_SR) & RTC_SR_ACKUPD) == 0)
		cpu_spinwait();
	WR4(sc, RTC_SCCR, RTC_SR_ACKUPD);

	/*
	 * Set the values in the hardware, then check whether the hardware was
	 * happy with them so we can return the correct status.
	 */
	WR4(sc, RTC_TIMR, RTC_TIMR_MK(ct.hour, ct.min, ct.sec));
	WR4(sc, RTC_CALR, RTC_CALR_MK(ct.year, ct.mon, ct.day, ct.dow+1));

	if (RD4(sc, RTC_VER) & (RTC_VER_NVTIM | RTC_VER_NVCAL))
		rv = EINVAL;
	else
		rv = 0;

	/*
	 * Restart the clocks (turn off the update bits).
	 * Clear the second-event bit (because the manual says to).
	 */
	WR4(sc, RTC_CR, RD4(sc, RTC_CR) & ~(RTC_CR_UPDCAL | RTC_CR_UPDTIM));
	WR4(sc, RTC_SCCR, RTC_SR_SECEV);

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

	DEVMETHOD_END
};

static driver_t at91_rtc_driver = {
	"at91_rtc",
	at91_rtc_methods,
	sizeof(struct at91_rtc_softc),
};

DRIVER_MODULE(at91_rtc, atmelarm, at91_rtc_driver, at91_rtc_devclass, 0, 0);
