/*-
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
 * $FreeBSD$
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

#include "clock_if.h"

#include <machine/cpuinfo.h>

#define NSEC_PER_SEC    1000000000L

static int	qemu_rtc_attach(device_t dev);
static int	qemu_rtc_probe(device_t dev);

static int	qemu_rtc_settime(device_t dev, struct timespec *ts);
static int	qemu_rtc_gettime(device_t dev, struct timespec *ts);

static device_method_t qemu_rtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		qemu_rtc_probe),
	DEVMETHOD(device_attach,	qemu_rtc_attach),

	/* clock interface */
	DEVMETHOD(clock_gettime,	qemu_rtc_gettime),
	DEVMETHOD(clock_settime,	qemu_rtc_settime),

	{ 0, 0 }
};

static driver_t qemu_rtc_driver = {
	"rtc",
	qemu_rtc_methods,
	0
};
static devclass_t qemu_rtc_devclass;
DRIVER_MODULE(rtc, nexus, qemu_rtc_driver, qemu_rtc_devclass, 0, 0);

static int
qemu_rtc_probe(device_t dev)
{

	if (!(cpuinfo.cpu_vendor == 0x0f && cpuinfo.cpu_rev >= 0x01))
		return (ENXIO);

	device_set_desc(dev, "Qemu Realtime Clock");
	return (BUS_PROBE_NOWILDCARD);
}

static int
qemu_rtc_attach(device_t dev)
{

	if (!(cpuinfo.cpu_vendor == 0x0f && cpuinfo.cpu_rev >= 0x01))
		return (ENXIO);

	clock_register(dev, 1000);
	return (0);
}

static int
qemu_rtc_settime(device_t dev, struct timespec *ts)
{

	if (!(cpuinfo.cpu_vendor == 0x0f && cpuinfo.cpu_rev >= 0x01))
		return (ENOTSUP);

	mips_wr_qemurtc64(ts->tv_sec * NSEC_PER_SEC + ts->tv_nsec);

	return (0);
}

static int
qemu_rtc_gettime(device_t dev, struct timespec *ts)
{
	int64_t nsecs;
	int32_t rem;

	if (!(cpuinfo.cpu_vendor == 0x0f && cpuinfo.cpu_rev >= 0x01))
		return (ENOTSUP);

	nsecs = mips_rd_qemurtc64();

	if (nsecs == 0) {
		ts->tv_sec = 0;
		ts->tv_nsec = 0;
	} else {
		ts->tv_sec = nsecs / NSEC_PER_SEC;
		rem = nsecs % NSEC_PER_SEC;
		if (rem < 0) {
			ts->tv_sec--;
			rem += NSEC_PER_SEC;
		}
		ts->tv_nsec = rem;
	}

	return (0);
}
