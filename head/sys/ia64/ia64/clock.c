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
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/priority.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/timetc.h>
#include <sys/pcpu.h>

#include <machine/cpu.h>
#include <machine/efi.h>
#include <machine/intr.h>
#include <machine/intrcnt.h>
#include <machine/md_var.h>
#include <machine/smp.h>

SYSCTL_NODE(_debug, OID_AUTO, clock, CTLFLAG_RW, 0, "clock statistics");

static int adjust_edges = 0;
SYSCTL_INT(_debug_clock, OID_AUTO, adjust_edges, CTLFLAG_RD,
    &adjust_edges, 0, "Number of times ITC got more than 12.5% behind");

static int adjust_excess = 0;
SYSCTL_INT(_debug_clock, OID_AUTO, adjust_excess, CTLFLAG_RD,
    &adjust_excess, 0, "Total number of ignored ITC interrupts");

static int adjust_lost = 0;
SYSCTL_INT(_debug_clock, OID_AUTO, adjust_lost, CTLFLAG_RD,
    &adjust_lost, 0, "Total number of lost ITC interrupts");

static int adjust_ticks = 0;
SYSCTL_INT(_debug_clock, OID_AUTO, adjust_ticks, CTLFLAG_RD,
    &adjust_ticks, 0, "Total number of ITC interrupts with adjustment");

static u_int ia64_clock_xiv;
static uint64_t ia64_clock_reload;

#ifndef SMP
static timecounter_get_t ia64_get_timecount;

static struct timecounter ia64_timecounter = {
	ia64_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"ITC"			/* name */
};

static u_int
ia64_get_timecount(struct timecounter* tc)
{
	return ia64_get_itc();
}
#endif

static u_int
ia64_ih_clock(struct thread *td, u_int xiv, struct trapframe *tf)
{
	uint64_t adj, clk, itc;
	int64_t delta;
	int count;

	PCPU_INC(md.stats.pcs_nclks);

	if (PCPU_GET(cpuid) == 0) {
		/*
		 * Clock processing on the BSP.
		 */
		intrcnt[INTRCNT_CLOCK]++;

		itc = ia64_get_itc();

		adj = PCPU_GET(md.clockadj);
		clk = PCPU_GET(md.clock);

		delta = itc - clk;
		count = 0;
		while (delta >= ia64_clock_reload) {
#ifdef SMP
			ipi_all_but_self(ia64_clock_xiv);
#endif
			hardclock(TRAPF_USERMODE(tf), TRAPF_PC(tf));
			if (profprocs != 0)
				profclock(TRAPF_USERMODE(tf), TRAPF_PC(tf));
			statclock(TRAPF_USERMODE(tf));
			delta -= ia64_clock_reload;
			clk += ia64_clock_reload;
			if (adj != 0)
				adjust_ticks++;
			count++;
		}
		ia64_set_itm(ia64_get_itc() + ia64_clock_reload - adj);
		ia64_srlz_d();
		if (count > 0) {
			adjust_lost += count - 1;
			if (delta > (ia64_clock_reload >> 3)) {
				if (adj == 0)
					adjust_edges++;
				adj = ia64_clock_reload >> 4;
			} else
				adj = 0;
		} else {
			adj = 0;
			adjust_excess++;
		}
		PCPU_SET(md.clock, clk);
		PCPU_SET(md.clockadj, adj);
	} else {
		/*
		 * Clock processing on the BSP.
		 */
		hardclock_cpu(TRAPF_USERMODE(tf));
		if (profprocs != 0)
			profclock(TRAPF_USERMODE(tf), TRAPF_PC(tf));
		statclock(TRAPF_USERMODE(tf));
	}

	return (0);
}

/*
 * Start the real-time and statistics clocks. We use ar.itc and cr.itm
 * to implement a 1000hz clock.
 */
void
cpu_initclocks()
{
	u_long itc_freq;

	ia64_clock_xiv = ia64_xiv_alloc(PI_REALTIME, IA64_XIV_IPI,
	    ia64_ih_clock);
	if (ia64_clock_xiv == 0)
		panic("No XIV for clock interrupts");

	itc_freq = (u_long)ia64_itc_freq() * 1000000ul;

	stathz = hz;
	ia64_clock_reload = (itc_freq + hz/2) / hz;

#ifndef SMP
	ia64_timecounter.tc_frequency = itc_freq;
	tc_init(&ia64_timecounter);
#endif

	PCPU_SET(md.clockadj, 0);
	PCPU_SET(md.clock, ia64_get_itc());
	ia64_set_itm(PCPU_GET(md.clock) + ia64_clock_reload);
	ia64_set_itv(ia64_clock_xiv);
	ia64_srlz_d();
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
