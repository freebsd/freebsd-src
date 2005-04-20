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
#include <sys/timetc.h>

#include <machine/clock.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/tick.h>
#include <machine/ver.h>

int tick_missed;	/* statistics */

#define	TICK_GRACE	10000

void
cpu_initclocks(void)
{

	stathz = hz;
	tick_start(tick_hardclock);
}

static __inline void
tick_process(struct clockframe *cf)
{

	if (PCPU_GET(cpuid) == 0)
		hardclock(cf);
	else
		hardclock_process(cf);
	if (profprocs != 0)
		profclock(cf);
	statclock(cf);
}

void
tick_hardclock(struct clockframe *cf)
{
	int missed;
	u_long next, s;

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
	s = intr_disable();
	while (next < rd(tick) + TICK_GRACE) {
		next += tick_increment;
		missed++;
	}
	wrtickcmpr(next, 0);
	intr_restore(s);
	atomic_add_int(&tick_missed, missed);
	for (; missed > 0; missed--)
		tick_process(cf);
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
}

void
tick_start(tick_func_t *func)
{
	u_long s;

	intr_setup(PIL_TICK, (ih_func_t *)func, -1, NULL, NULL);
	s = intr_disable();
	wrpr(tick, 0, 0);
	wrtickcmpr(tick_increment, 0);
	intr_restore(s);
}

#ifdef SMP
void
tick_start_ap(void)
{
	u_long base, s;

	/*
	 * Try to make the ticks interrupt as synchronously as possible to
	 * avoid inaccuracies for migrating processes. Leave out one tick to
	 * make sure that it is not missed.
	 */
	s = intr_disable();
	base = rd(tick);
	wrtickcmpr(roundup(base, tick_increment) + tick_increment, 0);
	intr_restore(s);
}
#endif

void
tick_stop(void)
{

	if (cpu_impl >= CPU_IMPL_ULTRASPARCIII)
		wr(asr24, 1L << 63, 0);
	wrtickcmpr(1L << 63, 0);
}
