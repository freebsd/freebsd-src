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
__FBSDID("$FreeBSD: src/sys/ia64/ia64/clock.c,v 1.32.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/clock.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/timetc.h>
#include <sys/pcpu.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/efi.h>

uint64_t ia64_clock_reload;

static int clock_initialized = 0;

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
	ia64_srlz_d();
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
	long days;
	struct efi_tm tm;
	struct timespec ts;
	struct clocktime ct;

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

	ct.nsec = tm.tm_nsec;
	ct.sec = tm.tm_sec;
	ct.min = tm.tm_min;
	ct.hour = tm.tm_hour;
	ct.day = tm.tm_mday;
	ct.mon = tm.tm_mon;
	ct.year = tm.tm_year;
	ct.dow = -1;
	if (clock_ct_to_ts(&ct, &ts))
		printf("Invalid time in clock: check and reset the date!\n");
	ts.tv_sec += utc_offset();

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
	struct timespec ts;
	struct clocktime ct;
	struct efi_tm tm;

	if (!clock_initialized || disable_rtc_set)
		return;

	efi_get_time(&tm);
	getnanotime(&ts);
	ts.tv_sec -= utc_offset();
	clock_ts_to_ct(&ts, &ct);

	tm.tm_nsec = ts.tv_nsec;
	tm.tm_sec = ct.sec;
	tm.tm_min = ct.min;
	tm.tm_hour = ct.hour;

	tm.tm_year = ct.year;
	tm.tm_mon = ct.mon;
	tm.tm_mday = ct.day;
	if (efi_set_time(&tm))
		printf("ERROR: COULD NOT RESET EFI CLOCK!\n");
}
