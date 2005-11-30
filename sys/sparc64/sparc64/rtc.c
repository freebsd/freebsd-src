/*-
 * Copyright (c) 2004 Marius Strobl <marius@FreeBSD.org>
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
 * The `rtc' device is a MC146818 compatible clock found on the ISA bus
 * and EBus. The EBus version also has an interrupt property so it could
 * be used to drive the statclock etc.
 */

#include "opt_isa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>

#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <isa/isavar.h>

#include <dev/mc146818/mc146818var.h>

#include "clock_if.h"

static devclass_t rtc_devclass;

static int	rtc_attach(device_t dev);
static int	rtc_ebus_probe(device_t dev);
#ifdef DEV_ISA
static int	rtc_isa_probe(device_t dev);
#endif

static device_method_t rtc_ebus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rtc_ebus_probe),
	DEVMETHOD(device_attach,	rtc_attach),

	/* clock interface */
	DEVMETHOD(clock_gettime,	mc146818_gettime),
	DEVMETHOD(clock_settime,	mc146818_settime),

	{ 0, 0 }
};

static driver_t rtc_ebus_driver = {
	"rtc",
	rtc_ebus_methods,
	sizeof(struct mc146818_softc),
};

DRIVER_MODULE(rtc, ebus, rtc_ebus_driver, rtc_devclass, 0, 0);

#ifdef DEV_ISA
static device_method_t rtc_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rtc_isa_probe),
	DEVMETHOD(device_attach,	rtc_attach),

	/* clock interface */
	DEVMETHOD(clock_gettime,	mc146818_gettime),
	DEVMETHOD(clock_settime,	mc146818_settime),

	{ 0, 0 }
};

static driver_t rtc_isa_driver = {
	"rtc",
	rtc_isa_methods,
	sizeof(struct mc146818_softc),
};

DRIVER_MODULE(rtc, isa, rtc_isa_driver, rtc_devclass, 0, 0);
#endif

static int
rtc_ebus_probe(device_t dev)
{
 
	if (strcmp(ofw_bus_get_name(dev), "rtc") == 0) {
		device_set_desc(dev, "Real Time Clock");
		return (0);
	}

	return (ENXIO);
}

#ifdef DEV_ISA
static struct isa_pnp_id rtc_isa_ids[] = {
	{ 0x000bd041, "AT realtime clock" }, /* PNP0B00 */
	{ 0 }
};

static int
rtc_isa_probe(device_t dev)
{

	if (ISA_PNP_PROBE(device_get_parent(dev), dev, rtc_isa_ids) == 0) {
		device_set_desc(dev, "Real Time Clock");
		return (0);
	}

	return (ENXIO);
}
#endif

static int
rtc_attach(device_t dev)
{
	struct timespec ts;
	struct mc146818_softc *sc;
	struct resource *res;
	int error, rid, rtype;

	sc = device_get_softc(dev);

	mtx_init(&sc->sc_mtx, "rtc_mtx", NULL, MTX_SPIN);

	if (strcmp(device_get_name(device_get_parent(dev)), "isa") == 0)
		rtype = SYS_RES_IOPORT;
	else
		rtype = SYS_RES_MEMORY;

	rid = 0;
	res = bus_alloc_resource_any(dev, rtype, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "cannot allocate resources\n");
		error = ENXIO;
		goto fail_mtx;
	}
	sc->sc_bst = rman_get_bustag(res);
	sc->sc_bsh = rman_get_bushandle(res);

	/* The TOD clock year 0 is 0. */
	sc->sc_year0 = 0;
	/* Use default register read/write and century get/set functions. */
	sc->sc_flag = MC146818_NO_CENT_ADJUST;
	if ((error = mc146818_attach(dev)) != 0) {
		device_printf(dev, "cannot attach time of day clock\n");
		goto fail_res;
	}

	if (bootverbose) {
		mc146818_gettime(dev, &ts);
		device_printf(dev, "current time: %ld.%09ld\n", (long)ts.tv_sec,
		    ts.tv_nsec);
        }

	return (0);

 fail_res:
	bus_release_resource(dev, rtype, rid, res);
 fail_mtx:
	mtx_destroy(&sc->sc_mtx);

	return (error);
}
