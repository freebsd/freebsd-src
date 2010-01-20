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
#include <machine/md_var.h>

uint64_t ia64_clock_reload;

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

	PCPU_SET(md.clockadj, 0);
	PCPU_SET(md.clock, ia64_get_itc());
	ia64_set_itm(PCPU_GET(md.clock) + ia64_clock_reload);
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
	u_long itc_freq;

	itc_freq = (u_long)ia64_itc_freq() * 1000000ul;

	stathz = hz;
	ia64_clock_reload = (itc_freq + hz/2) / hz;

#ifndef SMP
	ia64_timecounter.tc_frequency = itc_freq;
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
