/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>

#include "clock_if.h"

struct lpc_rtc_softc {
	device_t			lr_dev;
	struct resource	*		lr_mem_res;
	bus_space_tag_t			lr_bst;
	bus_space_handle_t		lr_bsh;
};

static int lpc_rtc_probe(device_t dev);
static int lpc_rtc_attach(device_t dev);
static int lpc_rtc_gettime(device_t dev, struct timespec *ts);
static int lpc_rtc_settime(device_t, struct timespec *);

static int
lpc_rtc_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "lpc,rtc"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 real time clock");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_rtc_attach(device_t dev)
{
	struct lpc_rtc_softc *sc = device_get_softc(dev);
	int rid = 0;

	sc->lr_dev = dev;

	clock_register(dev, 1000000);

	sc->lr_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->lr_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->lr_bst = rman_get_bustag(sc->lr_mem_res);
	sc->lr_bsh = rman_get_bushandle(sc->lr_mem_res);

	return (0);
}

static int
lpc_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct lpc_rtc_softc *sc = device_get_softc(dev);

	ts->tv_sec = bus_space_read_4(sc->lr_bst, sc->lr_bsh, LPC_RTC_UCOUNT);
	ts->tv_nsec = 0;

	return (0);
}

static int
lpc_rtc_settime(device_t dev, struct timespec *ts)
{
	struct lpc_rtc_softc *sc = device_get_softc(dev);
	uint32_t ctrl;

	/* Stop RTC */
	ctrl = bus_space_read_4(sc->lr_bst, sc->lr_bsh, LPC_RTC_CTRL);
	bus_space_write_4(sc->lr_bst, sc->lr_bsh, LPC_RTC_CTRL, ctrl | LPC_RTC_CTRL_DISABLE);

	/* Write actual value */
	bus_space_write_4(sc->lr_bst, sc->lr_bsh, LPC_RTC_UCOUNT, ts->tv_sec);

	/* Start RTC */
	ctrl = bus_space_read_4(sc->lr_bst, sc->lr_bsh, LPC_RTC_CTRL);
	bus_space_write_4(sc->lr_bst, sc->lr_bsh, LPC_RTC_CTRL, ctrl & ~LPC_RTC_CTRL_DISABLE);

	return (0);	
}

static device_method_t lpc_rtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_rtc_probe),
	DEVMETHOD(device_attach,	lpc_rtc_attach),

	/* Clock interface */
	DEVMETHOD(clock_gettime,	lpc_rtc_gettime),
	DEVMETHOD(clock_settime,	lpc_rtc_settime),

	{ 0, 0 },
};

static driver_t lpc_rtc_driver = {
	"rtc",
	lpc_rtc_methods,
	sizeof(struct lpc_rtc_softc),
};

static devclass_t lpc_rtc_devclass;

DRIVER_MODULE(rtc, simplebus, lpc_rtc_driver, lpc_rtc_devclass, 0, 0);
