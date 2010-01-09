/*-
 * Copyright (c) 2006-2009 RMI Corporation
 * Copyright (c) 2006 Bruce M. Simpson
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

/*
 * Simple driver for the 32-bit interval counter built in to all
 * MIPS32 CPUs.
 * XXX: For calibration this either needs an external clock, or
 * to be explicitly told what the frequency is.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/power.h>
#include <sys/smp.h>
#include <machine/clock.h>
#include <machine/locore.h>
#include <machine/md_var.h>
#include <machine/hwfunc.h>


struct timecounter counter_timecounter = {
	platform_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"MIPS32",		/* name */
	800,			/* quality (adjusted in code) */
};

void tick_init(void);


void
tick_init(void)
{
	counter_freq = platform_get_frequency();
	if (bootverbose)
		printf("MIPS32 clock: %u MHz", cpu_clock);

	counter_timecounter.tc_frequency = counter_freq;
	tc_init(&counter_timecounter);
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


static int
sysctl_machdep_counter_freq(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint64_t freq;

	/*
	 * RRS wonders if this will really work. You don't change the req of
	 * the system here, it would require changes to the RMI PIC in order
	 * to get the TC to run at a differrent frequency.
	 */

	if (counter_timecounter.tc_frequency == 0)
		return (EOPNOTSUPP);
	freq = counter_freq;
	error = sysctl_handle_int(oidp, &freq, sizeof(freq), req);
	if (error == 0 && req->newptr != NULL) {
		counter_freq = freq;
		counter_timecounter.tc_frequency = counter_freq;
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, counter_freq, CTLTYPE_QUAD | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_counter_freq, "IU", "");
