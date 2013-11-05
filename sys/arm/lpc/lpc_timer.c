/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timetc.h>
#include <sys/timeet.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

struct lpc_timer_softc {
	device_t		lt_dev;
	struct eventtimer	lt_et;
	struct resource	*	lt_res[5];
	bus_space_tag_t		lt_bst0;
	bus_space_handle_t	lt_bsh0;
	bus_space_tag_t		lt_bst1;
	bus_space_handle_t	lt_bsh1;
	int			lt_oneshot;
	uint32_t		lt_period;
};

static struct resource_spec lpc_timer_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ -1, 0 }
};

static struct lpc_timer_softc *timer_softc = NULL;
static int lpc_timer_initialized = 0;
static int lpc_timer_probe(device_t);
static int lpc_timer_attach(device_t);
static int lpc_timer_start(struct eventtimer *,
    sbintime_t first, sbintime_t period);
static int lpc_timer_stop(struct eventtimer *et);
static unsigned lpc_get_timecount(struct timecounter *);
static int lpc_hardclock(void *);

#define	timer0_read_4(sc, reg)			\
    bus_space_read_4(sc->lt_bst0, sc->lt_bsh0, reg)
#define	timer0_write_4(sc, reg, val)		\
    bus_space_write_4(sc->lt_bst0, sc->lt_bsh0, reg, val)
#define	timer0_clear(sc)			\
    do {					\
	    timer0_write_4(sc, LPC_TIMER_TC, 0);	\
	    timer0_write_4(sc, LPC_TIMER_PR, 0);	\
	    timer0_write_4(sc, LPC_TIMER_PC, 0);	\
    } while(0)

#define	timer1_read_4(sc, reg)			\
    bus_space_read_4(sc->lt_bst1, sc->lt_bsh1, reg)
#define	timer1_write_4(sc, reg, val)		\
    bus_space_write_4(sc->lt_bst1, sc->lt_bsh1, reg, val)
#define	timer1_clear(sc)			\
    do {					\
	    timer1_write_4(sc, LPC_TIMER_TC, 0);	\
	    timer1_write_4(sc, LPC_TIMER_PR, 0);	\
	    timer1_write_4(sc, LPC_TIMER_PC, 0);	\
    } while(0)

static struct timecounter lpc_timecounter = {
	.tc_get_timecount = lpc_get_timecount,
	.tc_name = "LPC32x0 Timer1",
	.tc_frequency = 0, /* will be filled later */
	.tc_counter_mask = ~0u,
	.tc_quality = 1000,
};

static int
lpc_timer_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "lpc,timer"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 timer");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_timer_attach(device_t dev)
{
	void *intrcookie;
	struct lpc_timer_softc *sc = device_get_softc(dev);
	phandle_t node;
	uint32_t freq;

	if (timer_softc)
		return (ENXIO);

	timer_softc = sc;

	if (bus_alloc_resources(dev, lpc_timer_spec, sc->lt_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->lt_bst0 = rman_get_bustag(sc->lt_res[0]);
	sc->lt_bsh0 = rman_get_bushandle(sc->lt_res[0]);
	sc->lt_bst1 = rman_get_bustag(sc->lt_res[1]);
	sc->lt_bsh1 = rman_get_bushandle(sc->lt_res[1]);

	if (bus_setup_intr(dev, sc->lt_res[2], INTR_TYPE_CLK,
	    lpc_hardclock, NULL, sc, &intrcookie)) {
		device_printf(dev, "could not setup interrupt handler\n");
		bus_release_resources(dev, lpc_timer_spec, sc->lt_res);
		return (ENXIO);
	}

	/* Enable timer clock */
	lpc_pwr_write(dev, LPC_CLKPWR_TIMCLK_CTRL1,
	    LPC_CLKPWR_TIMCLK_CTRL1_TIMER0 |
	    LPC_CLKPWR_TIMCLK_CTRL1_TIMER1);

	/* Get PERIPH_CLK encoded in parent bus 'bus-frequency' property */
	node = ofw_bus_get_node(dev);
	if (OF_getprop(OF_parent(node), "bus-frequency", &freq,
	    sizeof(pcell_t)) <= 0) {
		bus_release_resources(dev, lpc_timer_spec, sc->lt_res);
		bus_teardown_intr(dev, sc->lt_res[2], intrcookie);
		device_printf(dev, "could not obtain base clock frequency\n");
		return (ENXIO);
	}

	freq = fdt32_to_cpu(freq);

	/* Set desired frequency in event timer and timecounter */
	sc->lt_et.et_frequency = (uint64_t)freq;
	lpc_timecounter.tc_frequency = (uint64_t)freq;	

	sc->lt_et.et_name = "LPC32x0 Timer0";
	sc->lt_et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT;
	sc->lt_et.et_quality = 1000;
	sc->lt_et.et_min_period = (0x00000002LLU << 32) / sc->lt_et.et_frequency;
	sc->lt_et.et_max_period = (0xfffffffeLLU << 32) / sc->lt_et.et_frequency;
	sc->lt_et.et_start = lpc_timer_start;
	sc->lt_et.et_stop = lpc_timer_stop;
	sc->lt_et.et_priv = sc;

	et_register(&sc->lt_et);
	tc_init(&lpc_timecounter);

	/* Reset and enable timecounter */
	timer1_write_4(sc, LPC_TIMER_TCR, LPC_TIMER_TCR_RESET);
	timer1_write_4(sc, LPC_TIMER_TCR, 0);
	timer1_clear(sc);
	timer1_write_4(sc, LPC_TIMER_TCR, LPC_TIMER_TCR_ENABLE);

	/* DELAY() now can work properly */
	lpc_timer_initialized = 1;

	return (0);
}

static int
lpc_timer_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	struct lpc_timer_softc *sc = (struct lpc_timer_softc *)et->et_priv;
	uint32_t ticks;

	if (period == 0) {
		sc->lt_oneshot = 1;
		sc->lt_period = 0;
	} else {
		sc->lt_oneshot = 0;
		sc->lt_period = ((uint32_t)et->et_frequency * period) >> 32;
	}

	if (first == 0)
		ticks = sc->lt_period;
	else
		ticks = ((uint32_t)et->et_frequency * first) >> 32;

	/* Reset timer */
	timer0_write_4(sc, LPC_TIMER_TCR, LPC_TIMER_TCR_RESET);
	timer0_write_4(sc, LPC_TIMER_TCR, 0);
	
	/* Start timer */
	timer0_clear(sc);
	timer0_write_4(sc, LPC_TIMER_MR0, ticks);
	timer0_write_4(sc, LPC_TIMER_MCR, LPC_TIMER_MCR_MR0I | LPC_TIMER_MCR_MR0S);
	timer0_write_4(sc, LPC_TIMER_TCR, LPC_TIMER_TCR_ENABLE);
	return (0);
}

static int
lpc_timer_stop(struct eventtimer *et)
{
	struct lpc_timer_softc *sc = (struct lpc_timer_softc *)et->et_priv;

	timer0_write_4(sc, LPC_TIMER_TCR, 0);
	return (0);
}

static device_method_t lpc_timer_methods[] = {
	DEVMETHOD(device_probe,		lpc_timer_probe),
	DEVMETHOD(device_attach,	lpc_timer_attach),
	{ 0, 0 }
};

static driver_t lpc_timer_driver = {
	"timer",
	lpc_timer_methods,
	sizeof(struct lpc_timer_softc),
};

static devclass_t lpc_timer_devclass;

DRIVER_MODULE(timer, simplebus, lpc_timer_driver, lpc_timer_devclass, 0, 0);

static int
lpc_hardclock(void *arg)
{
	struct lpc_timer_softc *sc = (struct lpc_timer_softc *)arg;

	/* Reset pending interrupt */
	timer0_write_4(sc, LPC_TIMER_IR, 0xffffffff);

	/* Start timer again */
	if (!sc->lt_oneshot) {
		timer0_clear(sc);
		timer0_write_4(sc, LPC_TIMER_MR0, sc->lt_period);
		timer0_write_4(sc, LPC_TIMER_TCR, LPC_TIMER_TCR_ENABLE);
	}

	if (sc->lt_et.et_active)
		sc->lt_et.et_event_cb(&sc->lt_et, sc->lt_et.et_arg);

	return (FILTER_HANDLED);
}

static unsigned
lpc_get_timecount(struct timecounter *tc)
{
	return timer1_read_4(timer_softc, LPC_TIMER_TC);
}

void
cpu_initclocks(void)
{
	cpu_initclocks_bsp();
}

void
DELAY(int usec)
{
	uint32_t counter;
	uint32_t first, last;
	int val = (lpc_timecounter.tc_frequency / 1000000 + 1) * usec;

	/* Timer is not initialized yet */
	if (!lpc_timer_initialized) {
		for (; usec > 0; usec--)
			for (counter = 100; counter > 0; counter--)
				;
		return;
	}

	first = lpc_get_timecount(&lpc_timecounter);
	while (val > 0) {
		last = lpc_get_timecount(&lpc_timecounter);
		if (last < first) {
			/* Timer rolled over */
			last = first;
		}
		
		val -= (last - first);
		first = last;
	}
}
