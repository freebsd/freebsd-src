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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/timeet.h>
#include <sys/timetc.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/tick.h>

#ifdef DEBUG
#include <sys/proc.h>
#endif

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

static struct eventtimer tick_et;

static int tick_et_start(struct eventtimer *et,
    struct bintime *first, struct bintime *period);
static int tick_et_stop(struct eventtimer *et);
static void tick_intr(struct trapframe *);

static uint64_t
tick_cputicks(void)
{

	return (rd(tick));
}

void
cpu_initclocks(void)
{

	intr_setup(PIL_TICK, tick_intr, -1, NULL, NULL);

	tick_et.et_name = "tick";
	tick_et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT |
	    ET_FLAGS_PERCPU;
	tick_et.et_quality = 1000;
	tick_et.et_frequency = tick_freq;
	tick_et.et_min_period.sec = 0;
	tick_et.et_min_period.frac = 0x00010000LLU << 32; /* To be safe. */
	tick_et.et_max_period.sec = 3600 * 24; /* No practical limit. */
	tick_et.et_max_period.frac = 0;
	tick_et.et_start = tick_et_start;
	tick_et.et_stop = tick_et_stop;
	tick_et.et_priv = NULL;
	et_register(&tick_et);
	
	cpu_initclocks_bsp();
}

static __inline void
tick_process(struct trapframe *tf)
{
	struct trapframe *oldframe;
	struct thread *td;

	if (tick_et.et_active) {
		td = curthread;
		oldframe = td->td_intr_frame;
		td->td_intr_frame = tf;
		tick_et.et_event_cb(&tick_et, tick_et.et_arg);
		td->td_intr_frame = oldframe;
	}
}

static void
tick_intr(struct trapframe *tf)
{
	u_long adj, ref, s, tick, tick_increment;
	long delta;
	int count;

#ifdef DEBUG	
	if (curthread->td_critnest > 2 || curthread->td_critnest < 1)
		panic("nested hardclock %d\n", curthread->td_critnest);
#endif
	tick_increment = PCPU_GET(tickincrement);
	/*
	 * The sequence of reading the TICK register, calculating the value
	 * of the next tick and writing it to the TICK_CMPR register must not
	 * be interrupted, not even by an IPI, otherwise a value that is in
	 * the past could be written in the worst case, causing hardclock to
	 * stop.
	 */
	adj = PCPU_GET(tickadj);
	s = intr_disable_all();
	tick = rd(tick);
	if (tick_increment != 0)
		wrtickcmpr(tick + tick_increment - adj, 0);
	else
		wrtickcmpr(1L << 63, 0);
	intr_restore_all(s);

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
		if (tick_increment == 0)
			break;
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
}

void
tick_init(u_long clock)
{

	tick_freq = clock;
	tick_MHz = clock / 1000000;
	set_cputicker(tick_cputicks, tick_freq, 0);
}

static int
tick_et_start(struct eventtimer *et,
    struct bintime *first, struct bintime *period)
{
	u_long fdiv, div;
	u_long base;
	register_t s;

	if (period != NULL) {
		div = (tick_et.et_frequency * (period->frac >> 32)) >> 32;
		if (period->sec != 0)
			div += tick_et.et_frequency * period->sec;
	} else
		div = 0;
	if (first != NULL) {
		fdiv = (tick_et.et_frequency * (first->frac >> 32)) >> 32;
		if (first->sec != 0)
			fdiv += tick_et.et_frequency * first->sec;
	} else 
		fdiv = div;
	PCPU_SET(tickincrement, div);

	/*
	 * Try to make the tick interrupts as synchronously as possible on
	 * all CPUs to avoid inaccuracies for migrating processes. Leave out
	 * one tick to make sure that it is not missed.
	 */
	critical_enter();
	PCPU_SET(tickadj, 0);
	s = intr_disable_all();
	base = rd(tick);
	if (div != 0)
		base = roundup(base, div);
	PCPU_SET(tickref, base);
	wrtickcmpr(base + fdiv, 0);
	intr_restore_all(s);
	critical_exit();
	return (0);
}

static int
tick_et_stop(struct eventtimer *et)
{

	PCPU_SET(tickincrement, 0);
	wrtickcmpr(1L << 63, 0);
	return (0);
}

