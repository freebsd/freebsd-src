/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Arm Ltd
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
 * Driver for the Arm PL031 RTC device
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clock_if.h"

#define	RTCDR	0x00
#define	RTCMR	0x04
#define	RTCLR	0x08
#define	RTCCR	0x0c
#define	RTCIMSR	0x10
#define	RTCRIS	0x14
#define	RTCMIS	0x18
#define	RTCICR	0x1c

struct pl031_softc {
	struct resource	*reg;
	int reg_rid;
};

static device_probe_t pl031_probe;
static device_attach_t pl031_attach;
static device_detach_t pl031_detach;

static clock_gettime_t pl031_gettime;
static clock_settime_t pl031_settime;

static int
pl031_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "arm,pl031"))
		return (ENXIO);

	device_set_desc(dev, "PL031 RTC");
	return (BUS_PROBE_DEFAULT);
}

static int
pl031_attach(device_t dev)
{
	struct pl031_softc *sc;

	sc = device_get_softc(dev);

	sc->reg_rid = 0;
	sc->reg = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->reg_rid,
	    RF_ACTIVE);
	if (sc->reg == 0)
		return (ENXIO);

	clock_register(dev, 1000000);

	return (0);
}

static int
pl031_detach(device_t dev)
{
	struct pl031_softc *sc;

	sc = device_get_softc(dev);

	clock_unregister(dev);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->reg_rid, sc->reg);

	return (0);
}

static int
pl031_gettime(device_t dev, struct timespec *ts)
{
	struct pl031_softc *sc;

	sc = device_get_softc(dev);
	ts->tv_sec = bus_read_4(sc->reg, RTCDR);
	ts->tv_nsec = 0;

	return (0);
}

static int
pl031_settime(device_t dev, struct timespec *ts)
{
	struct pl031_softc *sc;

	sc = device_get_softc(dev);
	bus_write_4(sc->reg, RTCLR, ts->tv_sec);
	return (0);
}

static device_method_t pl031_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pl031_probe),
	DEVMETHOD(device_attach,	pl031_attach),
	DEVMETHOD(device_detach,	pl031_detach),

	/* Clock interface */
	DEVMETHOD(clock_gettime,	pl031_gettime),
	DEVMETHOD(clock_settime,	pl031_settime),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(pl031, pl031_driver, pl031_methods,
    sizeof(struct pl031_softc));

DRIVER_MODULE(pl031, simplebus, pl031_driver, 0, 0);
