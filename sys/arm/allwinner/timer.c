/*-
 * Copyright (c) 2012 Ganbold Tsagaankhuu <ganbold@gmail.com>
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <sys/kdb.h>

#include "a20/a20_cpu_cfg.h"

/**
 * Timer registers addr
 *
 */
#define SW_TIMER_IRQ_EN_REG 	0x00
#define SW_TIMER_IRQ_STA_REG 	0x04
#define SW_TIMER0_CTRL_REG 	0x10
#define SW_TIMER0_INT_VALUE_REG	0x14
#define SW_TIMER0_CUR_VALUE_REG	0x18

#define SW_COUNTER64LO_REG	0xa4
#define SW_COUNTER64HI_REG	0xa8
#define CNT64_CTRL_REG		0xa0

#define CNT64_RL_EN		0x02 /* read latch enable */

#define TIMER_ENABLE		(1<<0)
#define TIMER_AUTORELOAD	(1<<1)
#define TIMER_OSC24M		(1<<2) /* oscillator = 24mhz */
#define TIMER_PRESCALAR		(4<<4) /* prescalar = 16 */

#define SYS_TIMER_CLKSRC	24000000 /* clock source */

struct a10_timer_softc {
	device_t 	sc_dev;
	struct resource *res[2];
	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	void 		*sc_ih;		/* interrupt handler */
	uint32_t 	sc_period;
	uint32_t 	timer0_freq;
	struct eventtimer et;
	uint8_t 	sc_timer_type;	/* 0 for A10, 1 for A20 */
};

int a10_timer_get_timerfreq(struct a10_timer_softc *);

#define timer_read_4(sc, reg)	\
	bus_space_read_4(sc->sc_bst, sc->sc_bsh, reg)
#define timer_write_4(sc, reg, val)	\
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, reg, val)

static u_int	a10_timer_get_timecount(struct timecounter *);
static int	a10_timer_timer_start(struct eventtimer *,
    sbintime_t first, sbintime_t period);
static int	a10_timer_timer_stop(struct eventtimer *);

static uint64_t timer_read_counter64(void);

static int a10_timer_initialized = 0;
static int a10_timer_hardclock(void *);
static int a10_timer_probe(device_t);
static int a10_timer_attach(device_t);

static struct timecounter a10_timer_timecounter = {
	.tc_name           = "a10_timer timer0",
	.tc_get_timecount  = a10_timer_get_timecount,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = 1000,
};

struct a10_timer_softc *a10_timer_sc = NULL;

static struct resource_spec a10_timer_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static uint64_t
timer_read_counter64(void)
{
	uint32_t lo, hi;

	/* In case of A20 get appropriate counter info */
	if (a10_timer_sc->sc_timer_type)
		return (a20_read_counter64());

	/* Latch counter, wait for it to be ready to read. */
	timer_write_4(a10_timer_sc, CNT64_CTRL_REG, CNT64_RL_EN);
	while (timer_read_4(a10_timer_sc, CNT64_CTRL_REG) & CNT64_RL_EN)
		continue;

	hi = timer_read_4(a10_timer_sc, SW_COUNTER64HI_REG);
	lo = timer_read_4(a10_timer_sc, SW_COUNTER64LO_REG);

	return (((uint64_t)hi << 32) | lo);
}

static int
a10_timer_probe(device_t dev)
{
	struct a10_timer_softc *sc;

	sc = device_get_softc(dev);

	if (ofw_bus_is_compatible(dev, "allwinner,sun4i-timer"))
		sc->sc_timer_type = 0;
	else if (ofw_bus_is_compatible(dev, "allwinner,sun7i-timer"))
		sc->sc_timer_type = 1;
	else
		return (ENXIO);

	device_set_desc(dev, "Allwinner A10/A20 timer");
	return (BUS_PROBE_DEFAULT);
}

static int
a10_timer_attach(device_t dev)
{
	struct a10_timer_softc *sc;
	int err;
	uint32_t val;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, a10_timer_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->sc_dev = dev;
	sc->sc_bst = rman_get_bustag(sc->res[0]);
	sc->sc_bsh = rman_get_bushandle(sc->res[0]);

	/* Setup and enable the timer interrupt */
	err = bus_setup_intr(dev, sc->res[1], INTR_TYPE_CLK, a10_timer_hardclock,
	    NULL, sc, &sc->sc_ih);
	if (err != 0) {
		bus_release_resources(dev, a10_timer_spec, sc->res);
		device_printf(dev, "Unable to setup the clock irq handler, "
		    "err = %d\n", err);
		return (ENXIO);
	}

	/* Set clock source to OSC24M, 16 pre-division */
	val = timer_read_4(sc, SW_TIMER0_CTRL_REG);
	val |= TIMER_PRESCALAR | TIMER_OSC24M;
	timer_write_4(sc, SW_TIMER0_CTRL_REG, val);

	/* Enable timer0 */
	val = timer_read_4(sc, SW_TIMER_IRQ_EN_REG);
	val |= TIMER_ENABLE;
	timer_write_4(sc, SW_TIMER_IRQ_EN_REG, val);

	sc->timer0_freq = SYS_TIMER_CLKSRC;

	/* Set desired frequency in event timer and timecounter */
	sc->et.et_frequency = sc->timer0_freq;
	sc->et.et_name = "a10_timer Eventtimer";
	sc->et.et_flags = ET_FLAGS_ONESHOT | ET_FLAGS_PERIODIC;
	sc->et.et_quality = 1000;
	sc->et.et_min_period = (0x00000005LLU << 32) / sc->et.et_frequency;
	sc->et.et_max_period = (0xfffffffeLLU << 32) / sc->et.et_frequency;
	sc->et.et_start = a10_timer_timer_start;
	sc->et.et_stop = a10_timer_timer_stop;
	sc->et.et_priv = sc;
	et_register(&sc->et);

	if (device_get_unit(dev) == 0)
		a10_timer_sc = sc;

	a10_timer_timecounter.tc_frequency = sc->timer0_freq;
	tc_init(&a10_timer_timecounter);

	if (bootverbose) {
		device_printf(sc->sc_dev, "clock: hz=%d stathz = %d\n", hz, stathz);

		device_printf(sc->sc_dev, "event timer clock frequency %u\n", 
		    sc->timer0_freq);
		device_printf(sc->sc_dev, "timecounter clock frequency %lld\n", 
		    a10_timer_timecounter.tc_frequency);
	}

	a10_timer_initialized = 1;

	return (0);
}

static int
a10_timer_timer_start(struct eventtimer *et, sbintime_t first,
    sbintime_t period)
{
	struct a10_timer_softc *sc;
	uint32_t count;
	uint32_t val;

	sc = (struct a10_timer_softc *)et->et_priv;

	if (period != 0)
		sc->sc_period = ((uint32_t)et->et_frequency * period) >> 32;
	else
		sc->sc_period = 0;
	if (first != 0)
		count = ((uint32_t)et->et_frequency * first) >> 32;
	else
		count = sc->sc_period;

	/* Update timer values */
	timer_write_4(sc, SW_TIMER0_INT_VALUE_REG, sc->sc_period);
	timer_write_4(sc, SW_TIMER0_CUR_VALUE_REG, count);

	val = timer_read_4(sc, SW_TIMER0_CTRL_REG);
	if (period != 0) {
		/* periodic */
		val |= TIMER_AUTORELOAD;
	} else {
		/* oneshot */
		val &= ~TIMER_AUTORELOAD;
	}
	/* Enable timer0 */
	val |= TIMER_ENABLE;
	timer_write_4(sc, SW_TIMER0_CTRL_REG, val);

	return (0);
}

static int
a10_timer_timer_stop(struct eventtimer *et)
{
	struct a10_timer_softc *sc;
	uint32_t val;

	sc = (struct a10_timer_softc *)et->et_priv;

	/* Disable timer0 */
	val = timer_read_4(sc, SW_TIMER0_CTRL_REG);
	val &= ~TIMER_ENABLE;
	timer_write_4(sc, SW_TIMER0_CTRL_REG, val);

	sc->sc_period = 0;

	return (0);
}

int
a10_timer_get_timerfreq(struct a10_timer_softc *sc)
{
	return (sc->timer0_freq);
}

void
cpu_initclocks(void)
{
	cpu_initclocks_bsp();
}

static int
a10_timer_hardclock(void *arg)
{
	struct a10_timer_softc *sc;
	uint32_t val;

	sc = (struct a10_timer_softc *)arg;

	/* Clear interrupt pending bit. */
	timer_write_4(sc, SW_TIMER_IRQ_STA_REG, 0x1);

	val = timer_read_4(sc, SW_TIMER0_CTRL_REG);
	/*
	 * Disabled autoreload and sc_period > 0 means 
	 * timer_start was called with non NULL first value.
	 * Now we will set periodic timer with the given period 
	 * value.
	 */
	if ((val & (1<<1)) == 0 && sc->sc_period > 0) {
		/* Update timer */
		timer_write_4(sc, SW_TIMER0_CUR_VALUE_REG, sc->sc_period);

		/* Make periodic and enable */
		val |= TIMER_AUTORELOAD | TIMER_ENABLE;
		timer_write_4(sc, SW_TIMER0_CTRL_REG, val);
	}

	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}

u_int
a10_timer_get_timecount(struct timecounter *tc)
{

	if (a10_timer_sc == NULL)
		return (0);

	return ((u_int)timer_read_counter64());
}

static device_method_t a10_timer_methods[] = {
	DEVMETHOD(device_probe,		a10_timer_probe),
	DEVMETHOD(device_attach,	a10_timer_attach),

	DEVMETHOD_END
};

static driver_t a10_timer_driver = {
	"a10_timer",
	a10_timer_methods,
	sizeof(struct a10_timer_softc),
};

static devclass_t a10_timer_devclass;

DRIVER_MODULE(a10_timer, simplebus, a10_timer_driver, a10_timer_devclass, 0, 0);

void
DELAY(int usec)
{
	uint32_t counter;
	uint64_t end, now;

	if (!a10_timer_initialized) {
		for (; usec > 0; usec--)
			for (counter = 50; counter > 0; counter--)
				cpufunc_nullop();
		return;
	}

	now = timer_read_counter64();
	end = now + (a10_timer_sc->timer0_freq / 1000000) * (usec + 1);

	while (now < end)
		now = timer_read_counter64();
}

