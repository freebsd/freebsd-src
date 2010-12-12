/*-
 * Copyright (c) 2009 Adrian Chadd
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
 * $FreeBSD: stable/8/sys/i386/xen/xen_rtc.c 199583 2009-11-20 15:27:52Z jhb $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/8/sys/i386/xen/xen_rtc.c 199583 2009-11-20 15:27:52Z jhb $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>

#include <xen/xen_intr.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <xen/hypervisor.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xenfunc.h>
#include <xen/interface/io/xenbus.h>
#include <xen/interface/vcpu.h>
#include <machine/cpu.h>

#include <machine/xen/xen_clock_util.h>

#include "clock_if.h"

static int
xen_rtc_probe(device_t dev)
{
	device_set_desc(dev, "Xen Hypervisor Clock");
	printf("[XEN] xen_rtc_probe: probing Hypervisor RTC clock\n");
	if (! HYPERVISOR_shared_info) {
		device_printf(dev, "No hypervisor shared page found; RTC can not start.\n");
		return (EINVAL);
	}
	return (0);
}

static int
xen_rtc_attach(device_t dev)
{
	printf("[XEN] xen_rtc_attach: attaching Hypervisor RTC clock\n");
	clock_register(dev, 1000000);
	return(0);
}

static int
xen_rtc_settime(device_t dev __unused, struct timespec *ts)
{
	device_printf(dev, "[XEN] xen_rtc_settime\n");
	/*
	 * Don't return EINVAL here; just silently fail if the domain isn't privileged enough
	 * to set the TOD.
	 */
	return(0);
}

/*
 * The Xen time structures document the hypervisor start time and the
 * uptime-since-hypervisor-start (in nsec.) They need to be combined
 * in order to calculate a TOD clock.
 */
static int
xen_rtc_gettime(device_t dev, struct timespec *ts)
{
	struct timespec w_ts, u_ts;

	device_printf(dev, "[XEN] xen_rtc_gettime\n");
	xen_fetch_wallclock(&w_ts);
	device_printf(dev, "[XEN] xen_rtc_gettime: wallclock %ld sec; %ld nsec\n", (long int) w_ts.tv_sec, (long int) w_ts.tv_nsec);
	xen_fetch_uptime(&u_ts);
	device_printf(dev, "[XEN] xen_rtc_gettime: uptime %ld sec; %ld nsec\n", (long int) u_ts.tv_sec, (long int) u_ts.tv_nsec);

	timespecclear(ts);
	timespecadd(ts, &w_ts);
	timespecadd(ts, &u_ts);

	device_printf(dev, "[XEN] xen_rtc_gettime: TOD %ld sec; %ld nsec\n", (long int) ts->tv_sec, (long int) ts->tv_nsec);

	return(0);
}

static void
xen_rtc_identify(driver_t *drv, device_t parent)
{
        BUS_ADD_CHILD(parent, 0, "rtc", 0);
}

static device_method_t xen_rtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xen_rtc_probe),
	DEVMETHOD(device_attach,	xen_rtc_attach),
	DEVMETHOD(device_identify,	xen_rtc_identify),

	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* clock interface */
	DEVMETHOD(clock_gettime,	xen_rtc_gettime),
	DEVMETHOD(clock_settime,	xen_rtc_settime),

	{ 0, 0 }
};


static driver_t xen_rtc_driver = {
	"rtc",
	xen_rtc_methods,
	0
};

static devclass_t xen_rtc_devclass;

DRIVER_MODULE(rtc, nexus, xen_rtc_driver, xen_rtc_devclass, 0, 0);
