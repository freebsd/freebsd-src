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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>
#include <sys/timetc.h>
#ifdef SMP
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#endif

#include <dev/ofw/openfirm.h>

#include <machine/clock.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/tick.h>
#ifdef SMP
#include <machine/cpu.h>
#endif

int tick_missed;	/* statistics */

#define	TICK_GRACE	1000

void
cpu_initclocks(void)
{
	u_int clock;

	OF_getprop(PCPU_GET(node), "clock-frequency", &clock, sizeof(clock));
	tick_start(clock, tick_hardclock);
}

static __inline void
tick_process(struct clockframe *cf)
{

#ifdef SMP
	if (PCPU_GET(cpuid) == 0)
		hardclock(cf);
	else {
		CTR1(KTR_CLK, "tick_process: AP, cpuid=%d", PCPU_GET(cpuid));
		mtx_lock_spin_flags(&sched_lock, MTX_QUIET);
		hardclock_process(curthread, CLKF_USERMODE(cf));
		statclock_process(curthread->td_kse, CLKF_PC(cf),
		    CLKF_USERMODE(cf));
		mtx_unlock_spin_flags(&sched_lock, MTX_QUIET);
	}
#else
	hardclock(cf);
#endif
}

void
tick_hardclock(struct clockframe *cf)
{
	int missed;
	u_long next;

	tick_process(cf);
	/*
	 * Avoid stopping of hardclock in case we missed one tick period by
	 * ensuring that the the value of the next tick is at least TICK_GRACE
	 * ticks in the future.
	 * Missed ticks need to be accounted for by repeatedly calling
	 * hardclock.
	 */
	missed = 0;
	next = rd(asr23) + tick_increment;
	critical_enter();
	while (next < rd(tick) + TICK_GRACE) {
		next += tick_increment;
		missed++;
	}
	atomic_add_int(&tick_missed, missed);
	wr(asr23, next, 0);
	critical_exit();
	for (; missed > 0; missed--)
		tick_process(cf);
}

void
tick_start(u_long clock, tick_func_t *func)
{
	intr_setup(PIL_TICK, (ih_func_t *)func, -1, NULL, NULL);
	tick_freq = clock;
	tick_MHz = clock / 1000000;
	tick_increment = clock / hz;
	wrpr(tick, 0, 0);
	wr(asr23, clock / hz, 0);
}

#ifdef SMP
void
tick_start_ap(void)
{
	u_long base;

	/*
	 * Try to make the ticks interrupt as synchronously as possible to
	 * avoid inaccuracies for migrating processes. Leave out one tick to
	 * make sure that it is not missed.
	 */
	base = rd(tick);
	wr(asr23, roundup(base, tick_increment) + tick_increment, 0);
}
#endif

void
tick_stop(void)
{
	wrpr(tick, 0, 0);
	wr(asr23, 1L << 63, 0);
}
