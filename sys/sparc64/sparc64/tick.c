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
#include <sys/timetc.h>

#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/tick.h>

extern u_long tick_increment;
extern u_long tick_freq;
extern u_long tick_MHz;

int tick_missed;	/* statistics */

#define	TICK_GRACE	1000

void
tick_hardclock(struct clockframe *cf)
{
	critical_t c;
	int missed;
	u_long next;

	hardclock(cf);
	/*
	 * Avoid stopping of hardclock in case we missed one tick period by
	 * ensuring that the the value of the next tick is at least TICK_GRACE
	 * ticks in the future.
	 * Missed ticks need to be accounted for by repeatedly calling
	 * hardclock.
	 */
	missed = 0;
	next = rd(asr23) + tick_increment;
	c = critical_enter();
	while (next < rd(tick) + TICK_GRACE) {
		next += tick_increment;
		missed++;
	}
	atomic_add_int(&tick_missed, missed);
	wr(asr23, next, 0);
	critical_exit(c);
	for (; missed > 0; missed--)
		hardclock(cf);
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

void
tick_stop(void)
{
	wrpr(tick, 0, 0);
	wr(asr23, 1L << 63, 0);
}
