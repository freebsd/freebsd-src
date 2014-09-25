/*	$NetBSD: s3c24x0_clk.c,v 1.6 2005/12/24 20:06:52 perry Exp $ */

/*
 * Copyright (c) 2003  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
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
#include <arm/samsung/s3c2xx0/s3c24x0reg.h>
#include <arm/samsung/s3c2xx0/s3c24x0var.h>

struct s3c24x0_timer_softc {
	device_t	dev;
} timer_softc;

static unsigned s3c24x0_timer_get_timecount(struct timecounter *tc);

static struct timecounter s3c24x0_timer_timecounter = {
	s3c24x0_timer_get_timecount,	/* get_timecount */
	NULL,				/* no poll_pps */
	~0u,				/* counter_mask */
	3686400,			/* frequency */
	"s3c24x0 timer",		/* name */
	1000				/* quality */
};

static int
s3c24x0_timer_probe(device_t dev)
{

	device_set_desc(dev, "s3c24x0 timer");
	return (0);
}

static int
s3c24x0_timer_attach(device_t dev)
{
	timer_softc.dev = dev;

	/* We need to do this here for devices that expect DELAY to work */
	return (0);
}

static device_method_t s3c24x0_timer_methods[] = {
	DEVMETHOD(device_probe, s3c24x0_timer_probe),
	DEVMETHOD(device_attach, s3c24x0_timer_attach),
	{0, 0},
};

static driver_t s3c24x0_timer_driver = {
	"timer",
	s3c24x0_timer_methods,
	sizeof(struct s3c24x0_timer_softc),
};
static devclass_t s3c24x0_timer_devclass;

DRIVER_MODULE(s3c24x0timer, s3c24x0, s3c24x0_timer_driver,
    s3c24x0_timer_devclass, 0, 0);

#define TIMER_FREQUENCY(pclk) ((pclk)/16) /* divider=1/16 */

static unsigned int timer4_reload_value;
static unsigned int timer4_prescaler;
static unsigned int timer4_mseccount;
static volatile uint32_t s3c24x0_base;

#define usec_to_counter(t)	\
	((timer4_mseccount*(t))/1000)

#define counter_to_usec(c,pclk)	\
	(((c)*timer4_prescaler*1000)/(TIMER_FREQUENCY(pclk)/1000))

static inline int
read_timer(struct s3c24x0_softc *sc)
{
	int count;

	do {
		count = bus_space_read_2(sc->sc_sx.sc_iot, sc->sc_timer_ioh,
		    TIMER_TCNTO(4));
	} while ( __predict_false(count > timer4_reload_value) );

	return count;
}

static unsigned
s3c24x0_timer_get_timecount(struct timecounter *tc)
{
	struct s3c24x0_softc *sc = (struct s3c24x0_softc *)s3c2xx0_softc;
	int value;

	value = bus_space_read_2(sc->sc_sx.sc_iot, sc->sc_timer_ioh,
	    TIMER_TCNTO(4));
	return (s3c24x0_base - value);
}

static int
clock_intr(void *arg)
{
	struct trapframe *fp = arg;

	atomic_add_32(&s3c24x0_base, timer4_reload_value);

	hardclock(TRAPF_USERMODE(fp), TRAPF_PC(fp));
	return (FILTER_HANDLED);
}

void
cpu_initclocks(void)
{
	struct s3c24x0_softc *sc = (struct s3c24x0_softc *)s3c2xx0_softc;
	long tc;
	struct resource *irq;
	int rid = 0;
	void *ihl;
	int err, prescaler;
	int pclk = s3c2xx0_softc->sc_pclk;
	bus_space_tag_t iot = sc->sc_sx.sc_iot;
	bus_space_handle_t ioh = sc->sc_timer_ioh;
	uint32_t  reg;
	device_t dev = timer_softc.dev;

	/* We have already been initialized */
	if (timer4_reload_value != 0)
		return;

#define	time_constant(hz)	(TIMER_FREQUENCY(pclk) /(hz)/ prescaler)
#define calc_time_constant(hz)					\
	do {							\
		prescaler = 1;					\
		do {						\
			++prescaler;				\
			tc = time_constant(hz);			\
		} while( tc > 65536 );				\
	} while(0)


	/* Use the channels 4 and 3 for hardclock and statclock, respectively */

	/* stop all timers */
	bus_space_write_4(iot, ioh, TIMER_TCON, 0);

	/* calc suitable prescaler value */
	calc_time_constant(hz);

	timer4_prescaler = prescaler;
	timer4_reload_value = TIMER_FREQUENCY(pclk) / hz / prescaler;
	timer4_mseccount = TIMER_FREQUENCY(pclk)/timer4_prescaler/1000 ;

	bus_space_write_4(iot, ioh, TIMER_TCNTB(4),
	    ((prescaler - 1) << 16) | (timer4_reload_value - 1));

	printf("clock: hz=%d PCLK=%d prescaler=%d tc=%ld\n",
	    hz, pclk, prescaler, tc);

	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, S3C24X0_INT_TIMER4,
		S3C24X0_INT_TIMER4, 1, RF_ACTIVE);
	if (!irq)
		panic("Unable to allocate the clock irq handler.\n");

	err = bus_setup_intr(dev, irq, INTR_TYPE_CLK,
	    clock_intr, NULL, NULL, &ihl);
	if (err != 0)
		panic("Unable to setup the clock irq handler.\n");

	/* set prescaler1 */
	reg = bus_space_read_4(iot, ioh, TIMER_TCFG0);
	bus_space_write_4(iot, ioh, TIMER_TCFG0,
			  (reg & ~0xff00) | ((prescaler-1) << 8));

	/* divider 1/16 for ch #4 */
	reg = bus_space_read_4(iot, ioh, TIMER_TCFG1);
	bus_space_write_4(iot, ioh, TIMER_TCFG1,
			  (reg & ~(TCFG1_MUX_MASK(4))) |
			  (TCFG1_MUX_DIV16 << TCFG1_MUX_SHIFT(4)) );


	/* start timers */
	reg = bus_space_read_4(iot, ioh, TIMER_TCON);
	reg &= ~(TCON_MASK(4));

	/* load the time constant */
	bus_space_write_4(iot, ioh, TIMER_TCON, reg | TCON_MANUALUPDATE(4));
	/* set auto reload and start */
	bus_space_write_4(iot, ioh, TIMER_TCON, reg |
	    TCON_AUTORELOAD(4) | TCON_START(4) );

	s3c24x0_timer_timecounter.tc_frequency = TIMER_FREQUENCY(pclk) /
	    timer4_prescaler;
	tc_init(&s3c24x0_timer_timecounter);
}

/*
 * DELAY:
 *
 *	Delay for at least N microseconds.
 */
void
DELAY(int n)
{
	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) s3c2xx0_softc;
	int v0, v1, delta;
	u_int ucnt;

	if (timer4_reload_value == 0) {
		/* not initialized yet */
		while ( n-- > 0 ){
			int m;

			for (m = 0; m < 100; ++m )
				;
		}
		return;
	}

	/* read down counter */
	v0 = read_timer(sc);

	ucnt = usec_to_counter(n);

	while( ucnt > 0 ) {
		v1 = read_timer(sc);
		delta = v0 - v1;
		if ( delta < 0 )
			delta += timer4_reload_value;

		if((u_int)delta < ucnt){
			ucnt -= (u_int)delta;
			v0 = v1;
		}
		else {
			ucnt = 0;
		}
	}
}

void
cpu_startprofclock(void)
{
}

void
cpu_stopprofclock(void)
{
}
