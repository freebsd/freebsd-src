/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sparc64/sparc64/tick.c,v 1.22.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>
#include <sys/sysctl.h>
#include <sys/timetc.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/tick.h>
#include <machine/ver.h>

#define	TICK_GRACE	10000

SYSCTL_NODE(_machdep, OID_AUTO, tick, CTLFLAG_RD, 0, "tick statistics");

static int adjust_edges = 0;
SYSCTL_INT(_machdep_tick, OID_AUTO, adjust_edges, CTLFLAG_RD, &adjust_edges,
    0, "total number of times tick interrupts got more than 12.5% behind");

static int adjust_excess = 0;
SYSCTL_INT(_machdep_tick, OID_AUTO, adjust_excess, CTLFLAG_RD, &adjust_excess,
    0, "total number of ignored tick interrupts");

static int adjust_missed = 0;
SYSCTL_INT(_machdep_tick, OID_AUTO, adjust_missed, CTLFLAG_RD, &adjust_missed,
    0, "total number of missed tick interrupts");

static int adjust_ticks = 0;
SYSCTL_INT(_machdep_tick, OID_AUTO, adjust_ticks, CTLFLAG_RD, &adjust_ticks,
    0, "total number of tick interrupts with adjustment");

static void tick_hardclock(struct trapframe *tf);

static uint64_t
tick_cputicks(void)
{

	return (rd(tick));
}

void
cpu_initclocks(void)
{

	stathz = hz;
	tick_start();
}

static inline void
tick_process(struct trapframe *tf)
{

	if (curcpu == 0)
		hardclock(TRAPF_USERMODE(tf), TRAPF_PC(tf));
	else
		hardclock_cpu(TRAPF_USERMODE(tf));
	if (profprocs != 0)
		profclock(TRAPF_USERMODE(tf), TRAPF_PC(tf));
	statclock(TRAPF_USERMODE(tf));
}

static void
tick_hardclock(struct trapframe *tf)
{
	u_long adj, ref, tick;
	long delta;
	register_t s;
	int count;

	/*
	 * The sequence of reading the TICK register, calculating the value
	 * of the next tick and writing it to the TICK_CMPR register must not
	 * be interrupted, not even by an IPI, otherwise a value that is in
	 * the past could be written in the worst case, causing hardclock to
	 * stop.
	 */
	critical_enter();
	adj = PCPU_GET(tickadj);
	s = intr_disable();
	tick = rd(tick);
	wrtickcmpr(tick + tick_increment - adj, 0);
	intr_restore(s);
	ref = PCPU_GET(tickref);
	delta = tick - ref;
	count = 0;
	while (delta >= tick_increment) {
		tick_process(tf);
		delta -= tick_increment;
		ref += tick_increment;
		if (adj != 0)
			adjust_ticks++;
		count++;
	}
	if (count > 0) {
		adjust_missed += count - 1;
		if (delta > (tick_increment >> 3)) {
			if (adj == 0)
				adjust_edges++;
			adj = tick_increment >> 4;
		} else
			adj = 0;
	} else {
		adj = 0;
		adjust_excess++;
	}
	PCPU_SET(tickref, ref);
	PCPU_SET(tickadj, adj);
	critical_exit();
}

void
tick_init(u_long clock)
{

	tick_freq = clock;
	tick_MHz = clock / 1000000;
	tick_increment = clock / hz;

	/*
	 * UltraSparc II[e,i] based systems come up with the tick interrupt
	 * enabled and a handler that resets the tick counter, causing DELAY()
	 * to not work properly when used early in boot.
	 * UltraSPARC III based systems come up with the system tick interrupt
	 * enabled, causing an interrupt storm on startup since they are not
	 * handled.
	 */
	tick_stop();

	set_cputicker(tick_cputicks, tick_freq, 0);
}

void
tick_start(void)
{
	u_long base;
	register_t s;

	/*
	 * Avoid stopping of hardclock in terms of a lost tick interrupt
	 * by ensuring that the tick period is at least TICK_GRACE ticks.
	 * This check would be better placed in tick_init(), however we
	 * have to call tick_init() before cninit() in order to provide
	 * the low-level console drivers with a working DELAY() which in
	 * turn means we cannot use panic() in tick_init().
	 */
	if (tick_increment < TICK_GRACE)
		panic("%s: HZ too high, decrease to at least %ld", __func__,
		    tick_freq / TICK_GRACE);

	if (curcpu == 0)
		intr_setup(PIL_TICK, tick_hardclock, -1, NULL, NULL);

	/*
	 * Try to make the tick interrupts as synchronously as possible on
	 * all CPUs to avoid inaccuracies for migrating processes. Leave out
	 * one tick to make sure that it is not missed.
	 */
	PCPU_SET(tickadj, 0);
	s = intr_disable();
	base = rd(tick);
	base = roundup(base, tick_increment);
	PCPU_SET(tickref, base);
	wrtickcmpr(base + tick_increment, 0);
	intr_restore(s);
}

void
tick_stop(void)
{

	if (cpu_impl >= CPU_IMPL_ULTRASPARCIII)
		wr(asr25, 1L << 63, 0);
	wrtickcmpr(1L << 63, 0);
}
