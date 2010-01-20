/*-
 * Copyright (c) 2006 Kip Macy <kmacy@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <machine/hv_api.h>
#include <machine/mdesc_bus.h>
#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

#include "clock_if.h"

static int	hv_rtc_attach(device_t dev);
static int	hv_rtc_probe(device_t dev);
static int	hv_settime(device_t dev, struct timespec *ts);
static int	hv_gettime(device_t dev, struct timespec *ts);

static device_method_t hv_rtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		hv_rtc_probe),
	DEVMETHOD(device_attach,	hv_rtc_attach),

	/* clock interface */
	DEVMETHOD(clock_gettime,	hv_gettime),
	DEVMETHOD(clock_settime,	hv_settime),

	{ 0, 0 }
};

static driver_t hv_rtc_driver = {
	"hv_rtc",
	hv_rtc_methods,
	0
};
static devclass_t hv_rtc_devclass;
DRIVER_MODULE(rtc, vnex, hv_rtc_driver, hv_rtc_devclass, 0, 0);

static int
hv_rtc_probe(device_t dev)
{
	if (strcmp(mdesc_bus_get_name(dev), "rtc") == 0) {
		return (0);
	}

	return (ENXIO);
}

static int
hv_rtc_attach(device_t dev)
{
	clock_register(dev, 1000000);
	return (0);
}

static int
hv_settime(device_t dev, struct timespec *ts)
{
	hv_tod_set(ts->tv_sec);
	return (0);
}

static int
hv_gettime(device_t dev, struct timespec *ts)
{
	ts->tv_nsec = 0;
	hv_tod_get(&ts->tv_sec);
	return (0);
}
