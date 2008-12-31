/*	$NetBSD: ixp425_timer.c,v 1.11 2006/04/10 03:36:03 simonb Exp $ */

/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/ixp425/ixp425_timer.c,v 1.2.8.1 2008/11/25 02:59:29 kensmith Exp $");

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
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>
#include <machine/intr.h>
#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

static uint32_t counts_per_hz;

/* callback functions for intr_functions */
int	ixpclk_intr(void *);

struct ixpclk_softc {
	device_t		sc_dev;
	bus_addr_t		sc_baseaddr;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t      sc_ioh;
};

static unsigned ixp425_timer_get_timecount(struct timecounter *tc);

#ifndef IXP425_CLOCK_FREQ
#define	COUNTS_PER_SEC		66666600	/* 66MHz */
#else
#define	COUNTS_PER_SEC		IXP425_CLOCK_FREQ
#endif
#define	COUNTS_PER_USEC		((COUNTS_PER_SEC / 1000000) + 1)

static struct ixpclk_softc *ixpclk_sc = NULL;

#define GET_TS_VALUE(sc)	(*(volatile u_int32_t *) \
				  (IXP425_TIMER_VBASE + IXP425_OST_TS))

static struct timecounter ixp425_timer_timecounter = {
	ixp425_timer_get_timecount,	/* get_timecount */
	NULL, 				/* no poll_pps */
	~0u, 				/* counter_mask */
	COUNTS_PER_SEC,			/* frequency */
	"IXP425 Timer", 		/* name */
	1000,				/* quality */
};

static int
ixpclk_probe(device_t dev)
{
	device_set_desc(dev, "IXP425 Timer");
	return (0);
}

static int
ixpclk_attach(device_t dev)
{
	struct ixpclk_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));

	ixpclk_sc = sc;

	sc->sc_dev = dev;
	sc->sc_iot = sa->sc_iot;
	sc->sc_baseaddr = IXP425_TIMER_HWBASE;

	if (bus_space_map(sc->sc_iot, sc->sc_baseaddr, 8, 0,
			  &sc->sc_ioh))
		panic("%s: Cannot map registers", device_get_name(dev));

	return (0);
}

static device_method_t ixpclk_methods[] = {
	DEVMETHOD(device_probe, ixpclk_probe),
	DEVMETHOD(device_attach, ixpclk_attach),
	{0, 0},
};

static driver_t ixpclk_driver = {
	"ixpclk",
	ixpclk_methods,
	sizeof(struct ixpclk_softc),
};
static devclass_t ixpclk_devclass;

DRIVER_MODULE(ixpclk, ixp, ixpclk_driver, ixpclk_devclass, 0, 0);
static unsigned
ixp425_timer_get_timecount(struct timecounter *tc)
{
	uint32_t ret;

	ret = GET_TS_VALUE(sc);
	return (ret);
}

/*
 * cpu_initclocks:
 *
 *	Initialize the clock and get them going.
 */
void
cpu_initclocks(void)
{
	struct ixpclk_softc* sc = ixpclk_sc;
	struct resource *irq;
	device_t dev = sc->sc_dev;
	u_int oldirqstate;
	int rid = 0;
	void *ihl;

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

	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, IXP425_INT_TMR0,
	    IXP425_INT_TMR0, 1, RF_ACTIVE);
	if (!irq)
		panic("Unable to setup the clock irq handler.\n");
	else
		bus_setup_intr(dev, irq, INTR_TYPE_CLK, ixpclk_intr, NULL,
		    NULL, &ihl);

	/* Set up the new clock parameters. */

	/* clear interrupt */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IXP425_OST_STATUS,
			  OST_WARM_RESET | OST_WDOG_INT | OST_TS_INT |
			  OST_TIM1_INT | OST_TIM0_INT);

	counts_per_hz = COUNTS_PER_SEC / hz;

	/* reload value & Timer enable */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IXP425_OST_TIM0_RELOAD,
			  (counts_per_hz & TIMERRELOAD_MASK) | OST_TIMER_EN);

	tc_init(&ixp425_timer_timecounter);
	restore_interrupts(oldirqstate);
	rid = 0;
}


/*
 * DELAY:
 *
 *	Delay for at least N microseconds.
 */
void
DELAY(int n)
{
	u_int32_t first, last;
	int usecs;

	if (n == 0)
		return;

	/*
	 * Clamp the timeout at a maximum value (about 32 seconds with
	 * a 66MHz clock). *Nobody* should be delay()ing for anywhere
	 * near that length of time and if they are, they should be hung
	 * out to dry.
	 */
	if (n >= (0x80000000U / COUNTS_PER_USEC))
		usecs = (0x80000000U / COUNTS_PER_USEC) - 1;
	else
		usecs = n * COUNTS_PER_USEC;

	/* Note: Timestamp timer counts *up*, unlike the other timers */
	first = GET_TS_VALUE();

	while (usecs > 0) {
		last = GET_TS_VALUE();
		usecs -= (int)(last - first);
		first = last;
	}
}

/*
 * ixpclk_intr:
 *
 *	Handle the hardclock interrupt.
 */
int
ixpclk_intr(void *arg)
{
	struct ixpclk_softc* sc = ixpclk_sc;
	struct trapframe *frame = arg;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IXP425_OST_STATUS,
			  OST_TIM0_INT);

	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	return (FILTER_HANDLED);
}

void
cpu_startprofclock(void)
{
}

void
cpu_stopprofclock(void)
{
}
