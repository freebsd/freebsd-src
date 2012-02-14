/*	$NetBSD: sa11x0_ost.c,v 1.11 2003/07/15 00:24:51 lukem Exp $	*/

/*-
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by IWAMOTO Toshihiro and Ichiro FUKUHARA.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>

#include <arm/sa11x0/sa11x0_reg.h> 
#include <arm/sa11x0/sa11x0_var.h>
#include <arm/sa11x0/sa11x0_ostreg.h>

static int	saost_probe(device_t);
static int	saost_attach(device_t);

int		gettick(void);
static int	clockintr(void *);
#if 0
static int	statintr(void *);
#endif
void		rtcinit(void);

#if 0
static struct mtx clock_lock;
#endif

struct saost_softc {
	device_t		sc_dev;
	bus_addr_t		sc_baseaddr;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	u_int32_t	sc_clock_count;
	u_int32_t	sc_statclock_count;
	u_int32_t	sc_statclock_step;
};

static struct saost_softc *saost_sc = NULL;

#define TIMER_FREQUENCY         3686400         /* 3.6864MHz */
#define TICKS_PER_MICROSECOND   (TIMER_FREQUENCY/1000000)

#ifndef STATHZ
#define STATHZ	64
#endif

static device_method_t saost_methods[] = {
	DEVMETHOD(device_probe, saost_probe),
	DEVMETHOD(device_attach, saost_attach),
	{0, 0},
};

static driver_t saost_driver = {
	"saost",
	saost_methods,
	sizeof(struct saost_softc),
};
static devclass_t saost_devclass;

DRIVER_MODULE(saost, saip, saost_driver, saost_devclass, 0, 0);
static int
saost_probe(device_t dev)
{

	return (0);
}

static int
saost_attach(device_t dev)
{
	struct saost_softc *sc = device_get_softc(dev);
	struct sa11x0_softc *sa = device_get_softc(device_get_parent(dev));

	sc->sc_dev = dev;
	sc->sc_iot = sa->sc_iot;
	sc->sc_baseaddr = 0x90000000;

	saost_sc = sc;

	if(bus_space_map(sa->sc_iot, sc->sc_baseaddr, 8, 0, 
			&sc->sc_ioh))
		panic("%s: Cannot map registers", device_get_name(dev));

	/* disable all channel and clear interrupt status */
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_IR, 0);
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_SR, 0xf);
	return (0);

}

static int
clockintr(arg)
	void *arg;
{
	struct trapframe *frame = arg;
	u_int32_t oscr, nextmatch, oldmatch;
	int s;

#if 0
	mtx_lock_spin(&clock_lock);
#endif
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_SR, 1);

	/* schedule next clock intr */
	oldmatch = saost_sc->sc_clock_count;
	nextmatch = oldmatch + TIMER_FREQUENCY / hz;

	oscr = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
				SAOST_CR);

	if ((nextmatch > oldmatch &&
	     (oscr > nextmatch || oscr < oldmatch)) ||
	    (nextmatch < oldmatch && oscr > nextmatch && oscr < oldmatch)) {
		/*
		 * we couldn't set the matching register in time.
		 * just set it to some value so that next interrupt happens.
		 * XXX is it possible to compansate lost interrupts?
		 */

		s = splhigh();
		oscr = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
					SAOST_CR);
		nextmatch = oscr + 10;
		splx(s);
	}
	saost_sc->sc_clock_count = nextmatch;
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_MR0,
			  nextmatch);
	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
#if 0
	mtx_unlock_spin(&clock_lock);
#endif
	return (FILTER_HANDLED);
}

#if 0
static int
statintr(arg)
	void *arg;
{
	struct trapframe *frame = arg;
	u_int32_t oscr, nextmatch, oldmatch;
	int s;

	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_SR, 2);

	/* schedule next clock intr */
	oldmatch = saost_sc->sc_statclock_count;
	nextmatch = oldmatch + saost_sc->sc_statclock_step;

	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_MR1,
			  nextmatch);
	oscr = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
				SAOST_CR);

	if ((nextmatch > oldmatch &&
	     (oscr > nextmatch || oscr < oldmatch)) ||
	    (nextmatch < oldmatch && oscr > nextmatch && oscr < oldmatch)) {
		/*
		 * we couldn't set the matching register in time.
		 * just set it to some value so that next interrupt happens.
		 * XXX is it possible to compansate lost interrupts?
		 */

		s = splhigh();
		oscr = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
					SAOST_CR);
		nextmatch = oscr + 10;
		bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
				  SAOST_MR1, nextmatch);
		splx(s);
	}

	saost_sc->sc_statclock_count = nextmatch;
	statclock(TRAPF_USERMODE(frame));
	return (FILTER_HANDLED);
}
#endif

#if 0
void
setstatclockrate(int hz)
{
	u_int32_t count;

	saost_sc->sc_statclock_step = TIMER_FREQUENCY / hz;
	count = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_CR);
	count += saost_sc->sc_statclock_step;
	saost_sc->sc_statclock_count = count;
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_MR1, count);
}
#endif
void
cpu_initclocks()
{
	device_t dev = saost_sc->sc_dev;

	stathz = STATHZ;
	profhz = stathz;
#if 0
	mtx_init(&clock_lock, "SA1110 Clock locké", NULL, MTX_SPIN);
#endif
	saost_sc->sc_statclock_step = TIMER_FREQUENCY / stathz;
	struct resource *irq1, *irq2;
	int rid = 0;
	void *ih1/*, *ih2 */;
	
	printf("clock: hz=%d stathz = %d\n", hz, stathz);

	/* Use the channels 0 and 1 for hardclock and statclock, respectively */
	saost_sc->sc_clock_count = TIMER_FREQUENCY / hz;
	saost_sc->sc_statclock_count = TIMER_FREQUENCY / stathz;

	irq1 = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0,
	    ~0, 1, RF_ACTIVE);
	rid = 1;
	irq2 = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_ACTIVE);
	bus_setup_intr(dev, irq1, INTR_TYPE_CLK, clockintr, NULL, NULL,
	    &ih1);
#if 0
	bus_setup_intr(dev, irq2, INTR_TYPE_CLK, statintr, NULL, NULL,
	    ,&ih2);
#endif
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_SR, 0xf);
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_IR, 3);
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_MR0,
			  saost_sc->sc_clock_count);
#if 0
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_MR1,
			  0);
#endif
	/* Zero the counter value */
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_CR, 0);
}

int
gettick()
{
	int counter;
	u_int savedints;
	savedints = disable_interrupts(I32_bit);

	counter = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_CR);

	restore_interrupts(savedints);
	return counter;
}

void
DELAY(usecs)
	int usecs;
{
	u_int32_t tick, otick, delta;
	int j, csec, usec;

	csec = usecs / 10000;
	usec = usecs % 10000;
	
	usecs = (TIMER_FREQUENCY / 100) * csec
	    + (TIMER_FREQUENCY / 100) * usec / 10000;

	if (! saost_sc) {
		/* clock isn't initialized yet */
		for(; usecs > 0; usecs--)
			for(j = 100; j > 0; j--)
				;
		return;
	}

#if 0
	mtx_lock_spin(&clock_lock);
#endif
	otick = gettick();

	while (1) {
		for(j = 100; j > 0; j--)
			;
		tick = gettick();
		delta = tick - otick;
		if (delta > usecs) {
			break;
		}
		usecs -= delta;
		otick = tick;
	}
#if 0
	mtx_unlock_spin(&clock_lock);
#endif
}

void
cpu_startprofclock(void)
{
	printf("STARTPROFCLOCK\n");
}

void
cpu_stopprofclock(void)
{
}
