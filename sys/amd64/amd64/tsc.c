/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)clock.c	7.2 (Berkeley) 5/12/91
 * $FreeBSD$
 */

/*
 * Routines to handle clock hardware.
 */

/*
 * inittodr, settodr and support routines written
 * by Christoph Robitschko <chmr@edvz.tu-graz.ac.at>
 *
 * reintroduced and updated by Chris Stenton <chris@gnome.co.uk> 8/10/94
 */

#include "opt_clock.h"

#include <sys/param.h>
#include <sys/stdint.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/power.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

uint64_t	tsc_freq;
int		tsc_is_broken;
u_int		tsc_present;

static	unsigned tsc_get_timecount(struct timecounter *tc);

static struct timecounter tsc_timecounter = {
	tsc_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
 	~0u,			/* counter_mask */
	0,			/* frequency */
	 "TSC"			/* name */
};

void
init_TSC(void)
{
	u_int64_t tscval[2];

	if (cpu_feature & CPUID_TSC)
		tsc_present = 1;
	else
		tsc_present = 0;

	if (!tsc_present) 
		return;

	if (bootverbose)
	        printf("Calibrating TSC clock ... ");

	tscval[0] = rdtsc();
	DELAY(1000000);
	tscval[1] = rdtsc();

	tsc_freq = tscval[1] - tscval[0];
	if (bootverbose)
		printf("TSC clock: %ju Hz\n", (intmax_t)tsc_freq);

#if defined(SMP)
	/*
	 * We can not use the TSC in SMP mode, until we figure out a
	 * cheap (impossible), reliable and precise (yeah right!)  way
	 * to synchronize the TSCs of all the CPUs.
	 * Modern SMP hardware has the ACPI timer and we use that.
	 */
	return;
#endif

	/*
	 * We can not use the TSC if we support APM. Precise timekeeping
	 * on an APM'ed machine is at best a fools pursuit, since 
	 * any and all of the time spent in various SMM code can't 
	 * be reliably accounted for.  Reading the RTC is your only
	 * source of reliable time info.  The i8254 looses too of course
	 * but we need to have some kind of time...
	 * We don't know at this point whether APM is going to be used
	 * or not, nor when it might be activated.  Play it safe.
	 */
	if (power_pm_get_type() == POWER_PM_TYPE_APM) {
		if (bootverbose)
			printf("TSC timecounter disabled: APM enabled.\n");
		return;
	}

	if (tsc_present && tsc_freq != 0 && !tsc_is_broken) {
		tsc_timecounter.tc_frequency = tsc_freq;
		tc_init(&tsc_timecounter);
	}

	return;
}

static int
sysctl_machdep_tsc_freq(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint64_t freq;

	if (tsc_timecounter.tc_frequency == 0)
		return (EOPNOTSUPP);
	freq = tsc_freq;
	error = sysctl_handle_int(oidp, &freq, sizeof(freq), req);
	if (error == 0 && req->newptr != NULL) {
		tsc_freq = freq;
		tsc_timecounter.tc_frequency = tsc_freq;
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, tsc_freq, CTLTYPE_QUAD | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_tsc_freq, "IU", "");

static unsigned
tsc_get_timecount(struct timecounter *tc)
{
	return (rdtsc());
}
