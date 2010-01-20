/*	$NetBSD: i80321_timer.c,v 1.7 2003/07/27 04:52:28 thorpej Exp $	*/

/*-
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Timer/clock support for the Intel i80321 I/O processor.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>
#include <arm/xscale/i80321/i80321reg.h>
#include <arm/xscale/i80321/i80321var.h>

#include <arm/xscale/xscalevar.h>

#include "opt_timer.h"

void (*i80321_hardclock_hook)(void) = NULL;
struct i80321_timer_softc {
	device_t	dev;
} timer_softc;


static unsigned i80321_timer_get_timecount(struct timecounter *tc);
	

static uint32_t counts_per_hz;

static uint32_t offset = 0;
static int32_t last = -1;
static int ticked = 0;

#ifndef COUNTS_PER_SEC
#define	COUNTS_PER_SEC		200000000	/* 200MHz */
#endif
#define	COUNTS_PER_USEC		(COUNTS_PER_SEC / 1000000)

static struct timecounter i80321_timer_timecounter = {
	i80321_timer_get_timecount, /* get_timecount */
	NULL,			    /* no poll_pps */
	~0u,			    /* counter_mask */
	COUNTS_PER_SEC,	 	   /* frequency */
	"i80321 timer",		    /* name */
	1000			    /* quality */
};

static int
i80321_timer_probe(device_t dev)
{

	device_set_desc(dev, "i80321 timer");
	return (0);
}

static int
i80321_timer_attach(device_t dev)
{
	timer_softc.dev = dev;

	return (0);
}

static device_method_t i80321_timer_methods[] = {
	DEVMETHOD(device_probe, i80321_timer_probe),
	DEVMETHOD(device_attach, i80321_timer_attach),
	{0, 0},
};

static driver_t i80321_timer_driver = {
	"itimer",
	i80321_timer_methods,
	sizeof(struct i80321_timer_softc),
};
static devclass_t i80321_timer_devclass;

DRIVER_MODULE(itimer, iq, i80321_timer_driver, i80321_timer_devclass, 0, 0);

void	counterhandler(void *);
void	clockhandler(void *);


static __inline uint32_t
tmr1_read(void)
{
	uint32_t rv;

	__asm __volatile("mrc p6, 0, %0, c1, c1, 0"
		: "=r" (rv));
	return (rv);
}

static __inline void
tmr1_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c1, c1, 0"
		:
		: "r" (val));
}

static __inline uint32_t
tcr1_read(void)
{
	uint32_t rv;

	__asm __volatile("mrc p6, 0, %0, c3, c1, 0"
		: "=r" (rv));
	return (rv);
}
static __inline void
tcr1_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c3, c1, 0"
		:
		: "r" (val));
}

static __inline void
trr1_write(uint32_t val)
{

	__asm __volatile("mcr p6, 1, %0, c5, c1, 0"
		:
		: "r" (val));
}

static __inline uint32_t
tmr0_read(void)
{
	uint32_t rv;

	__asm __volatile("mrc p6, 0, %0, c0, c1, 0"
		: "=r" (rv));
	return (rv);
}

static __inline void
tmr0_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c0, c1, 0"
		:
		: "r" (val));
}

static __inline uint32_t
tcr0_read(void)
{
	uint32_t rv;

	__asm __volatile("mrc p6, 0, %0, c2, c1, 0"
		: "=r" (rv));
	return (rv);
}
static __inline void
tcr0_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c2, c1, 0"
		:
		: "r" (val));
}

static __inline void
trr0_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c4, c1, 0"
		:
		: "r" (val));
}

static __inline void
tisr_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c6, c1, 0"
		:
		: "r" (val));
}

static __inline uint32_t
tisr_read(void)
{
	int ret;
	
	__asm __volatile("mrc p6, 0, %0, c6, c1, 0" : "=r" (ret));
	return (ret);
}

static unsigned
i80321_timer_get_timecount(struct timecounter *tc)
{
	int32_t cur = tcr0_read();
	
	if (cur > last && last != -1) {
		offset += counts_per_hz;
		if (ticked > 0)
									                        ticked--;
	}
	if (ticked) {
		offset += ticked * counts_per_hz;
		ticked = 0;
	}
	last = cur;
	return (counts_per_hz - cur + offset);
}
/*
 * i80321_calibrate_delay:
 *
 *	Calibrate the delay loop.
 */
void
i80321_calibrate_delay(void)
{

	/*
	 * Just use hz=100 for now -- we'll adjust it, if necessary,
	 * in cpu_initclocks().
	 */
	counts_per_hz = COUNTS_PER_SEC / 100;

	tmr0_write(0);			/* stop timer */
	tisr_write(TISR_TMR0);		/* clear interrupt */
	trr0_write(counts_per_hz);	/* reload value */
	tcr0_write(counts_per_hz);	/* current value */

	tmr0_write(TMRx_ENABLE|TMRx_RELOAD|TMRx_CSEL_CORE);
}

/*
 * cpu_initclocks:
 *
 *	Initialize the clock and get them going.
 */
void
cpu_initclocks(void)
{
	u_int oldirqstate;
	struct resource *irq;
	int rid = 0;
	void *ihl;
	device_t dev = timer_softc.dev;

	if (hz < 50 || COUNTS_PER_SEC % hz) {
		printf("Cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
	tick = 1000000 / hz;	/* number of microseconds between interrupts */

	/*
	 * We only have one timer available; stathz and profhz are
	 * always left as 0 (the upper-layer clock code deals with
	 * this situation).
	 */
	if (stathz != 0)
		printf("Cannot get %d Hz statclock\n", stathz);
	stathz = 0;

	if (profhz != 0)
		printf("Cannot get %d Hz profclock\n", profhz);
	profhz = 0;

	/* Report the clock frequency. */

	oldirqstate = disable_interrupts(I32_bit);

	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, ICU_INT_TMR0,
	    ICU_INT_TMR0, 1, RF_ACTIVE);
	if (!irq)
		panic("Unable to setup the clock irq handler.\n");
	else
		bus_setup_intr(dev, irq, INTR_TYPE_CLK | INTR_FAST, 
		    clockhandler, NULL, &ihl);
	tmr0_write(0);			/* stop timer */
	tisr_write(TISR_TMR0);		/* clear interrupt */

	counts_per_hz = COUNTS_PER_SEC / hz;

	trr0_write(counts_per_hz);	/* reload value */
	tcr0_write(counts_per_hz);	/* current value */
	tmr0_write(TMRx_ENABLE|TMRx_RELOAD|TMRx_CSEL_CORE);

	tc_init(&i80321_timer_timecounter);
	restore_interrupts(oldirqstate);
}


/*
 * DELAY:
 *
 *	Delay for at least N microseconds.
 */
void
DELAY(int n)
{
	uint32_t cur, last, delta, usecs;

	/*
	 * This works by polling the timer and counting the
	 * number of microseconds that go by.
	 */
	last = tcr0_read();
	delta = usecs = 0;

	while (n > usecs) {
		cur = tcr0_read();

		/* Check to see if the timer has wrapped around. */
		if (last < cur)
			delta += (last + (counts_per_hz - cur));
		else
			delta += (last - cur);

		last = cur;

		if (delta >= COUNTS_PER_USEC) {
			usecs += delta / COUNTS_PER_USEC;
			delta %= COUNTS_PER_USEC;
		}
	}
}

/*
 * clockhandler:
 *
 *	Handle the hardclock interrupt.
 */
void
clockhandler(void *arg)
{
	struct clockframe *frame = arg;

	ticked++;
	tisr_write(TISR_TMR0);
	hardclock(frame);

	if (i80321_hardclock_hook != NULL)
		(*i80321_hardclock_hook)();
	return;
}

void
cpu_startprofclock(void)
{
}

void
cpu_stopprofclock(void)
{
	
}
