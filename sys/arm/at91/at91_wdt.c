/*-
 * Copyright (c) 2010 Greg Ansley.  All rights reserved.
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
 * The SAM9 watchdog hardware can be programed only once. So we set the
 * hardware watchdog to 16 s in wdt_attach and only reset it in the wdt_tick
 * handler.  The watchdog is halted in processor debug mode.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/watchdog.h>

#include <machine/bus.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_wdtreg.h>

struct wdt_softc {
	struct mtx	sc_mtx;
	device_t	sc_dev;
	struct resource	*mem_res;
	struct callout	tick_ch;
	eventhandler_tag sc_wet;
	void		*intrhand;
	u_int		cmd;
	u_int		interval;
};

static inline uint32_t
RD4(struct wdt_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct wdt_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

static int
wdt_intr(void *argp)
{
	struct wdt_softc *sc = argp;


	if (RD4(sc, WDT_SR) & (WDT_WDUNF | WDT_WDERR)) {
#if defined(KDB) && !defined(KDB_UNATTENDED)
		kdb_backtrace();
		kdb_enter(KDB_WHY_WATCHDOG, "watchdog timeout");
#else
		panic("watchdog timeout");
#endif
	}
	return (FILTER_STRAY);
}

/* User interface, see watchdog(9) */
static void
wdt_watchdog(void *argp, u_int cmd, int *error)
{
	struct wdt_softc *sc = argp;
	u_int interval;

	mtx_lock(&sc->sc_mtx);

	*error = 0;
	sc->cmd = 0;
	interval = cmd & WD_INTERVAL;
	if (interval > WD_TO_16SEC)
		*error = EOPNOTSUPP;
	else if (interval > 0)
		sc->cmd = interval | WD_ACTIVE;

	/* We cannot turn off our watchdog so if user
	 * fails to turn us on go to passive mode. */
	if ((sc->cmd & WD_ACTIVE) == 0)
		sc->cmd = WD_PASSIVE;

	mtx_unlock(&sc->sc_mtx);
}

/* This routine is called no matter what state the user sets the
 * watchdog mode to. Called at a rate that is slightly less than
 * half the hardware timeout. */
static void
wdt_tick(void *argp)
{
	struct wdt_softc *sc = argp;

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	if (sc->cmd & (WD_ACTIVE | WD_PASSIVE))
		WR4(sc, WDT_CR, WDT_KEY|WDT_WDRSTT);

	sc->cmd &= WD_PASSIVE;
	callout_reset(&sc->tick_ch, sc->interval, wdt_tick, sc);
}

static int
wdt_probe(device_t dev)
{

	if (at91_is_sam9() || at91_is_sam9xe()) {
		device_set_desc(dev, "WDT");
		return (0);
	}
	return (ENXIO);
}

static int
wdt_attach(device_t dev)
{
	static struct wdt_softc *sc;
	struct resource *irq;
	uint32_t wdt_mr;
	int rid, err;

	sc = device_get_softc(dev);
	sc->cmd = WD_PASSIVE;
	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), "at91_wdt", MTX_DEF);
	callout_init_mtx(&sc->tick_ch, &sc->sc_mtx, 0);

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	if (sc->mem_res == NULL)
		panic("couldn't allocate wdt register resources");

	wdt_mr = RD4(sc, WDT_MR);
	if ((wdt_mr & WDT_WDRSTEN) == 0)
		device_printf(dev, "Watchdog disabled! (Boot ROM?)\n");
	else {
#ifdef WDT_RESET
		/* Rude, full reset of whole system on watch dog timeout */
		WR4(sc, WDT_MR, WDT_WDDBGHLT | WDT_WDD(0xC00)|
		    WDT_WDRSTEN| WDT_WDV(0xFFF));
#else
		/* Generate stack trace and panic on watchdog timeout*/
		WR4(sc, WDT_MR, WDT_WDDBGHLT | WDT_WDD(0xC00)|
		    WDT_WDFIEN| WDT_WDV(0xFFF));
#endif
		/*
		 * This may have been set by Boot ROM so register value may
		 * not be what we just requested since this is a write once
		 * register.
		 */
		wdt_mr = RD4(sc, WDT_MR);
		if (wdt_mr & WDT_WDFIEN) {
			rid = 0;
			irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
			    RF_ACTIVE | RF_SHAREABLE);
			if (!irq)
				panic("could not allocate interrupt.\n");

			err = bus_setup_intr(dev, irq, INTR_TYPE_CLK, wdt_intr,
			    NULL, sc, &sc->intrhand);
		}

		/* interval * hz */
		sc->interval = (((wdt_mr & WDT_WDV(~0)) + 1) * WDT_DIV) /
		    (WDT_CLOCK/hz);

		device_printf(dev, "watchdog timeout: %d seconds\n",
		    sc->interval / hz);

		/* Slightly less than 1/2 of watchdog hardware timeout */
		sc->interval = (sc->interval/2) - (sc->interval/20);
		callout_reset(&sc->tick_ch, sc->interval, wdt_tick, sc);

		/* Register us as a watchdog */
		sc->sc_wet = EVENTHANDLER_REGISTER(watchdog_list,
		    wdt_watchdog, sc, 0);
	}
	return (0);
}

static device_method_t wdt_methods[] = {
	DEVMETHOD(device_probe, wdt_probe),
	DEVMETHOD(device_attach, wdt_attach),
	DEVMETHOD_END
};

static driver_t wdt_driver = {
	"at91_wdt",
	wdt_methods,
	sizeof(struct wdt_softc),
};

static devclass_t wdt_devclass;

DRIVER_MODULE(at91_wdt, atmelarm, wdt_driver, wdt_devclass, NULL, NULL);
