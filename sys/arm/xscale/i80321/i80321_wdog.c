/*	$NetBSD: i80321_wdog.c,v 1.6 2003/07/15 00:24:54 lukem Exp $	*/

/*-
 * Copyright (c) 2005 Olivier Houchard
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Watchdog timer support for the Intel i80321 I/O processor.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/watchdog.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/machdep.h>

#include <arm/xscale/i80321/i80321reg.h>
#include <arm/xscale/i80321/i80321var.h>


struct iopwdog_softc {
	device_t dev;
	int armed;
	int wdog_period;
	struct callout_handle wdog_callout;
};

static __inline void
wdtcr_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c7, c1, 0"
		:
		: "r" (val));
}

static void
iopwdog_tickle(void *arg)
{
	struct iopwdog_softc *sc = arg;

	if (!sc->armed)
		return;
	wdtcr_write(WDTCR_ENABLE1);
	wdtcr_write(WDTCR_ENABLE2);
	sc->wdog_callout = timeout(iopwdog_tickle, sc,
	    hz * (sc->wdog_period - 1));
}

static int
iopwdog_probe(device_t dev)
{
	struct iopwdog_softc *sc = device_get_softc(dev);
	char buf[128];

	/*
	 * XXX Should compute the period based on processor speed.
	 * For a 600MHz XScale core, the wdog must be tickled approx.
	 * every 7 seconds.
	 */

	sc->wdog_period = 7;
	sprintf(buf, "i80321 Watchdog, must be tickled every %d seconds",
	    sc->wdog_period);
	device_set_desc_copy(dev, buf);

	return (0);
}

static void
iopwdog_watchdog_fn(void *private, u_int cmd, int *error)
{
	struct iopwdog_softc *sc = private;

	if (cmd == 0)
		return;
	if ((((uint64_t)1 << (cmd & WD_INTERVAL))) >
	    (uint64_t)sc->wdog_period * 1000000000)
		return;
	sc->armed = 1;
	iopwdog_tickle(sc);
	*error = 0;
}

static int
iopwdog_attach(device_t dev)
{
	struct iopwdog_softc *sc = device_get_softc(dev);
	
	sc->dev = dev;
	sc->armed = 0;
	EVENTHANDLER_REGISTER(watchdog_list, iopwdog_watchdog_fn, sc, 0);
	return (0);
}

static device_method_t iopwdog_methods[] = {
	DEVMETHOD(device_probe, iopwdog_probe),
	DEVMETHOD(device_attach, iopwdog_attach),
	{0, 0},
};

static driver_t iopwdog_driver = {
	"iopwdog",
	iopwdog_methods,
	sizeof(struct iopwdog_softc),
};
static devclass_t iopwdog_devclass;

DRIVER_MODULE(iopwdog, iq, iopwdog_driver, iopwdog_devclass, 0, 0);
