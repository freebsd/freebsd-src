/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: Utah $Hdr: clock.c 1.18 91/01/21$
 *	from: @(#)clock.c	8.2 (Berkeley) 1/12/94
 *	from: NetBSD: clock_subr.c,v 1.6 2001/07/07 17:04:02 thorpej Exp
 *	and
 *	from: src/sys/i386/isa/clock.c,v 1.176 2001/09/04
 */

/*
 * Helpers for time-of-day clocks. This is useful for architectures that need
 * support multiple models of such clocks, and generally serves to make the
 * code more machine-independent.
 * If the clock in question can also be used as a time counter, the driver
 * needs to initiate this.
 * This code is not yet used by all architectures.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/sysctl.h>
#include <sys/timetc.h>

#include "clock_if.h"

static device_t clock_dev = NULL;
static long clock_res;
static struct timespec clock_adj;

/* XXX: should be kern. now, it's no longer machdep.  */
static int disable_rtc_set;
SYSCTL_INT(_machdep, OID_AUTO, disable_rtc_set, CTLFLAG_RW, &disable_rtc_set,
    0, "Disallow adjusting time-of-day clock");

void
clock_register(device_t dev, long res)	/* res has units of microseconds */
{

	if (clock_dev != NULL) {
		if (clock_res > res) {
			if (bootverbose)
				device_printf(dev, "not installed as "
				    "time-of-day clock: clock %s has higher "
				    "resolution\n", device_get_name(clock_dev));
			return;
		}
		if (bootverbose)
			device_printf(clock_dev, "removed as "
			    "time-of-day clock: clock %s has higher "
			    "resolution\n", device_get_name(dev));
	}
	clock_dev = dev;
	clock_res = res;
	clock_adj.tv_sec = res / 2 / 1000000;
	clock_adj.tv_nsec = res / 2 % 1000000 * 1000;
	if (bootverbose)
		device_printf(dev, "registered as a time-of-day clock "
		    "(resolution %ldus, adjustment %jd.%09jds)\n", res,
		    (intmax_t)clock_adj.tv_sec, (intmax_t)clock_adj.tv_nsec);
}

/*
 * inittodr and settodr derived from the i386 versions written
 * by Christoph Robitschko <chmr@edvz.tu-graz.ac.at>,  reintroduced and
 * updated by Chris Stenton <chris@gnome.co.uk> 8/10/94
 */

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(time_t base)
{
	struct timespec ts;
	int error;

	if (clock_dev == NULL) {
		printf("warning: no time-of-day clock registered, system time "
		    "will not be set accurately\n");
		goto wrong_time;
	}
	/* XXX: We should poll all registered RTCs in case of failure */
	error = CLOCK_GETTIME(clock_dev, &ts);
	if (error != 0 && error != EINVAL) {
		printf("warning: clock_gettime failed (%d), the system time "
		    "will not be set accurately\n", error);
		goto wrong_time;
	}
	if (error == EINVAL || ts.tv_sec < 0) {
		printf("Invalid time in real time clock.\n"
		    "Check and reset the date immediately!\n");
		goto wrong_time;
	}

	ts.tv_sec += utc_offset();
	timespecadd(&ts, &clock_adj);
	tc_setclock(&ts);
	return;

wrong_time:
	if (base > 0) {
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
	}
}

/*
 * Write system time back to RTC
 */
void
resettodr(void)
{
	struct timespec ts;
	int error;

	if (disable_rtc_set || clock_dev == NULL)
		return;

	getnanotime(&ts);
	timespecadd(&ts, &clock_adj);
	ts.tv_sec -= utc_offset();
	/* XXX: We should really set all registered RTCs */
	if ((error = CLOCK_SETTIME(clock_dev, &ts)) != 0)
		printf("warning: clock_settime failed (%d), time-of-day clock "
		    "not adjusted to system time\n", error);
}
