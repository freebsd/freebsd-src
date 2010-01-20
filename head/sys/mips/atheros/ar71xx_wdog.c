/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * Watchdog driver for AR71xx 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/watchdog.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <mips/atheros/ar71xxreg.h>

struct ar71xx_wdog_softc {
	device_t dev;
	int armed;
};

static void
ar71xx_wdog_watchdog_fn(void *private, u_int cmd, int *error)
{
	struct ar71xx_wdog_softc *sc = private;
	uint64_t timer_val;

	cmd &= WD_INTERVAL;
	if (cmd > 0) {
		timer_val = (uint64_t)(1ULL << cmd) * ar71xx_ahb_freq() /
		    1000000000;
		/*
		 * Load timer with large enough value to prevent spurious
		 * reset
		 */
		ATH_WRITE_REG(AR71XX_RST_WDOG_TIMER, 
		    ar71xx_ahb_freq() * 10);
		ATH_WRITE_REG(AR71XX_RST_WDOG_CONTROL, 
		    RST_WDOG_ACTION_RESET);
		ATH_WRITE_REG(AR71XX_RST_WDOG_TIMER, 
		    (timer_val & 0xffffffff));
		sc->armed = 1;
		*error = 0;
	} else {
		if (sc->armed) {
			ATH_WRITE_REG(AR71XX_RST_WDOG_CONTROL, 
			    RST_WDOG_ACTION_NOACTION);
			sc->armed = 0;
		}
	}
}

static int
ar71xx_wdog_probe(device_t dev)
{

	device_set_desc(dev, "Atheros AR71XX watchdog timer");
	return (0);
}

static int
ar71xx_wdog_attach(device_t dev)
{
	struct ar71xx_wdog_softc *sc = device_get_softc(dev);
	
	if (ATH_READ_REG(AR71XX_RST_WDOG_CONTROL) & RST_WDOG_LAST)
		device_printf (dev, 
		    "Previous reset was due to watchdog timeout\n");

	ATH_WRITE_REG(AR71XX_RST_WDOG_CONTROL, RST_WDOG_ACTION_NOACTION);

	sc->dev = dev;
	EVENTHANDLER_REGISTER(watchdog_list, ar71xx_wdog_watchdog_fn, sc, 0);

	return (0);
}

static device_method_t ar71xx_wdog_methods[] = {
	DEVMETHOD(device_probe, ar71xx_wdog_probe),
	DEVMETHOD(device_attach, ar71xx_wdog_attach),
	{0, 0},
};

static driver_t ar71xx_wdog_driver = {
	"ar71xx_wdog",
	ar71xx_wdog_methods,
	sizeof(struct ar71xx_wdog_softc),
};
static devclass_t ar71xx_wdog_devclass;

DRIVER_MODULE(ar71xx_wdog, nexus, ar71xx_wdog_driver, ar71xx_wdog_devclass, 0, 0);
