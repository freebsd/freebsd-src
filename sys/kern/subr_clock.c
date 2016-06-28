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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/limits.h>
#include <sys/sysctl.h>
#include <sys/timetc.h>

int tz_minuteswest;
int tz_dsttime;

/*
 * The adjkerntz and wall_cmos_clock sysctls are in the "machdep" sysctl
 * namespace because they were misplaced there originally.
 */
static int adjkerntz;
static int
sysctl_machdep_adjkerntz(SYSCTL_HANDLER_ARGS)
{
	int error;
	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
	if (!error && req->newptr)
		resettodr();
	return (error);
}
SYSCTL_PROC(_machdep, OID_AUTO, adjkerntz, CTLTYPE_INT | CTLFLAG_RW |
    CTLFLAG_MPSAFE, &adjkerntz, 0, sysctl_machdep_adjkerntz, "I",
    "Local offset from UTC in seconds");

static int ct_debug;
SYSCTL_INT(_debug, OID_AUTO, clocktime, CTLFLAG_RW,
    &ct_debug, 0, "Enable printing of clocktime debugging");

static int wall_cmos_clock;
SYSCTL_INT(_machdep, OID_AUTO, wall_cmos_clock, CTLFLAG_RW,
    &wall_cmos_clock, 0, "Enables application of machdep.adjkerntz");

/*--------------------------------------------------------------------*
 * Generic routines to convert between a POSIX date
 * (seconds since 1/1/1970) and yr/mo/day/hr/min/sec
 * Derived from NetBSD arch/hp300/hp300/clock.c
 */


#define	FEBRUARY	2
#define	days_in_year(y) 	(leapyear(y) ? 366 : 365)
#define	days_in_month(y, m) \
	(month_days[(m) - 1] + (m == FEBRUARY ? leapyear(y) : 0))
/* Day of week. Days are counted from 1/1/1970, which was a Thursday */
#define	day_of_week(days)	(((days) + 4) % 7)

static const int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};


/*
 * This inline avoids some unnecessary modulo operations
 * as compared with the usual macro:
 *   ( ((year % 4) == 0 &&
 *      (year % 100) != 0) ||
 *     ((year % 400) == 0) )
 * It is otherwise equivalent.
 */
static int
leapyear(int year)
{
	int rv = 0;

	if ((year & 3) == 0) {
		rv = 1;
		if ((year % 100) == 0) {
			rv = 0;
			if ((year % 400) == 0)
				rv = 1;
		}
	}
	return (rv);
}

static void
print_ct(struct clocktime *ct)
{
	printf("[%04d-%02d-%02d %02d:%02d:%02d]",
	    ct->year, ct->mon, ct->day,
	    ct->hour, ct->min, ct->sec);
}

int
clock_ct_to_ts(struct clocktime *ct, struct timespec *ts)
{
	int i, year, days;

	year = ct->year;

	if (ct_debug) {
		printf("ct_to_ts(");
		print_ct(ct);
		printf(")");
	}

	/* Sanity checks. */
	if (ct->mon < 1 || ct->mon > 12 || ct->day < 1 ||
	    ct->day > days_in_month(year, ct->mon) ||
	    ct->hour > 23 ||  ct->min > 59 || ct->sec > 59 ||
	    (sizeof(time_t) == 4 && year > 2037)) {	/* time_t overflow */
		if (ct_debug)
			printf(" = EINVAL\n");
		return (EINVAL);
	}

	/*
	 * Compute days since start of time
	 * First from years, then from months.
	 */
	days = 0;
	for (i = POSIX_BASE_YEAR; i < year; i++)
		days += days_in_year(i);

	/* Months */
	for (i = 1; i < ct->mon; i++)
	  	days += days_in_month(year, i);
	days += (ct->day - 1);

	ts->tv_sec = (((time_t)days * 24 + ct->hour) * 60 + ct->min) * 60 +
	    ct->sec;
	ts->tv_nsec = ct->nsec;

	if (ct_debug)
		printf(" = %ld.%09ld\n", (long)ts->tv_sec, (long)ts->tv_nsec);
	return (0);
}

void
clock_ts_to_ct(struct timespec *ts, struct clocktime *ct)
{
	int i, year, days;
	time_t rsec;	/* remainder seconds */
	time_t secs;

	secs = ts->tv_sec;
	days = secs / SECDAY;
	rsec = secs % SECDAY;

	ct->dow = day_of_week(days);

	/* Subtract out whole years, counting them in i. */
	for (year = POSIX_BASE_YEAR; days >= days_in_year(year); year++)
		days -= days_in_year(year);
	ct->year = year;

	/* Subtract out whole months, counting them in i. */
	for (i = 1; days >= days_in_month(year, i); i++)
		days -= days_in_month(year, i);
	ct->mon = i;

	/* Days are what is left over (+1) from all that. */
	ct->day = days + 1;

	/* Hours, minutes, seconds are easy */
	ct->hour = rsec / 3600;
	rsec = rsec % 3600;
	ct->min  = rsec / 60;
	rsec = rsec % 60;
	ct->sec  = rsec;
	ct->nsec = ts->tv_nsec;
	if (ct_debug) {
		printf("ts_to_ct(%ld.%09ld) = ",
		    (long)ts->tv_sec, (long)ts->tv_nsec);
		print_ct(ct);
		printf("\n");
	}
}

int
utc_offset(void)
{

	return (tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0));
}
