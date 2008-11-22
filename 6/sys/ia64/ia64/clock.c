/*-
 * Copyright (c) 2005 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/timetc.h>
#include <sys/pcpu.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/efi.h>

static int sysctl_machdep_adjkerntz(SYSCTL_HANDLER_ARGS);

int disable_rtc_set;		/* disable resettodr() if != 0 */
SYSCTL_INT(_machdep, CPU_DISRTCSET, disable_rtc_set,
	CTLFLAG_RW, &disable_rtc_set, 0, "");

int wall_cmos_clock;		/* wall	CMOS clock assumed if != 0 */
SYSCTL_INT(_machdep, CPU_WALLCLOCK, wall_cmos_clock,
	CTLFLAG_RW, &wall_cmos_clock, 0, "");

int adjkerntz;			/* local offset	from GMT in seconds */
SYSCTL_PROC(_machdep, CPU_ADJKERNTZ, adjkerntz, CTLTYPE_INT|CTLFLAG_RW,
	&adjkerntz, 0, sysctl_machdep_adjkerntz, "I", "");

static int
sysctl_machdep_adjkerntz(SYSCTL_HANDLER_ARGS)
{
	int error;

	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
	if (!error && req->newptr)
		resettodr();
	return (error);
}

uint64_t ia64_clock_reload;

static int clock_initialized = 0;

static short dayyr[12] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/*
 * Leap years
 *
 * Our well-known calendar, the Gregorian calendar, is intended to be of the
 * same length as the cycle of the seasons (the tropical year). However, the
 * tropical year is approximately 365.2422 days. If the calendar year always
 * consisted of 365 days, it would be short of the tropical year by about
 * 0.2422 days every year. Over a century, the beginning of spring in the
 * northern hemisphere would shift from March 20 to April 13.
 *
 * When Pope Gregory XIII instituted the Gregorian calendar in 1582, the
 * calendar was shifted to make the beginning of spring fall on March 21 and
 * a new system of leap days was introduced. Instead of intercalating a leap
 * day every fourth year, 97 leap days would be introduced every 400 years,
 * according to the following rule:
 *
 *     Years evenly divisible by 4 are leap years, with the exception of
 *     centurial years that are not evenly divisible by 400.
 *
 * Thus, the average Gregorian calendar year is 365.2425 days in length. This
 * agrees to within half a minute of the length of the tropical year.
 */

static __inline
int isleap(int yr)
{

	return ((yr % 4) ? 0 : (yr % 100) ? 1 : (yr % 400) ? 0 : 1);
}

#ifndef SMP
static timecounter_get_t ia64_get_timecount;

static struct timecounter ia64_timecounter = {
	ia64_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"ITC"			/* name */
};

static unsigned
ia64_get_timecount(struct timecounter* tc)
{
	return ia64_get_itc();
}
#endif

void
pcpu_initclock(void)
{

	PCPU_SET(clockadj, 0);
	PCPU_SET(clock, ia64_get_itc());
	ia64_set_itm(PCPU_GET(clock) + ia64_clock_reload);
	ia64_set_itv(CLOCK_VECTOR);	/* highest priority class */
}

/*
 * Start the real-time and statistics clocks. We use cr.itc and cr.itm
 * to implement a 1000hz clock.
 */
void
cpu_initclocks()
{

	if (itc_frequency == 0)
		panic("Unknown clock frequency");

	stathz = hz;
	ia64_clock_reload = (itc_frequency + hz/2) / hz;

#ifndef SMP
	ia64_timecounter.tc_frequency = itc_frequency;
	tc_init(&ia64_timecounter);
#endif

	pcpu_initclock();
}

void
cpu_startprofclock(void)
{

	/* nothing to do */
}

void
cpu_stopprofclock(void)
{

	/* nothing to do */
}

void
inittodr(time_t base)
{
	struct efi_tm tm;
	struct timespec ts;
	long days;
	int yr;

	efi_get_time(&tm);

	/*
	 * This code was written in 2005, so logically EFI cannot return
	 * a year smaller than that. Assume the EFI clock is out of whack
	 * in that case and reset the EFI clock.
	 */
	if (tm.tm_year < 2005) {
		printf("WARNING: CHECK AND RESET THE DATE!\n");
		memset(&tm, 0, sizeof(tm));
		tm.tm_year = 2005;
		tm.tm_mon = tm.tm_mday = 1;
		if (efi_set_time(&tm))
			printf("ERROR: COULD NOT RESET EFI CLOCK!\n");
	}

	days = 0L;
	for (yr = 1970; yr < (int)tm.tm_year; yr++)
		days += isleap(yr) ? 366L : 365L;
	days += dayyr[tm.tm_mon - 1] + tm.tm_mday - 1L;
	if (isleap(tm.tm_year) && tm.tm_mon > 2)
		days++;

	ts.tv_sec = ((days * 24L + tm.tm_hour) * 60L + tm.tm_min) * 60L
	    + tm.tm_sec + ((wall_cmos_clock) ? adjkerntz : 0L);
	ts.tv_nsec = tm.tm_nsec;

	/*
	 * The EFI clock is supposed to be a real-time clock, whereas the
	 * base argument is coming from a saved (as on disk) time. It's
	 * impossible for a saved time to represent a time in the future,
	 * so we expect the EFI clock to be larger. If not, the EFI clock
	 * may not be reliable and we trust the base.
	 * Warn if the EFI clock was off by 2 or more days.
	 */
	if (ts.tv_sec < base) {
		days = (base - ts.tv_sec) / (60L * 60L * 24L);
		if (days >= 2)
			printf("WARNING: EFI clock lost %ld days!\n", days);
		ts.tv_sec = base;
		ts.tv_nsec = 0;
	}

	tc_setclock(&ts);
	clock_initialized = 1;
}

/*
 * Reset the TODR based on the time value; used when the TODR has a
 * preposterous value and also when the time is reset by the stime
 * system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECYEAR+2*SECDAY) (e.g. on Jan 2 just after midnight)
 * to wrap the TODR around.
 */
void
resettodr()
{
	struct efi_tm tm;
	long t;
	int x;

	if (!clock_initialized || disable_rtc_set)
		return;

	efi_get_time(&tm);
	tm.tm_nsec = 0;

	t = time_second - ((wall_cmos_clock) ? adjkerntz : 0L);

	tm.tm_sec = t % 60;	t /= 60L;
	tm.tm_min = t % 60;	t /= 60L;
	tm.tm_hour = t % 24;	t /= 24L;

	tm.tm_year = 1970;
	x = (isleap(tm.tm_year)) ? 366 : 365;
	while (t > x) {
		t -= x;
		tm.tm_year++;
		x = (isleap(tm.tm_year)) ? 366 : 365;
	}

	x = 11;
	while (t < dayyr[x])
		x--;
	tm.tm_mon = x + 1;
	tm.tm_mday = t - dayyr[x] + 1;
	if (efi_set_time(&tm))
		printf("ERROR: COULD NOT RESET EFI CLOCK!\n");
}
