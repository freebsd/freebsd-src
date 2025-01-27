/*-
 * Copyright (c) 2012 Ganbold Tsagaankhuu <ganbold@freebsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/machdep.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>

#if defined(__aarch64__) || defined(__riscv)
#include "opt_soc.h"
#else
#include <arm/allwinner/aw_machdep.h>
#endif

/**
 * Timer registers addr
 *
 */
#define	TIMER_IRQ_EN_REG 	0x00
#define	 TIMER_IRQ_ENABLE(x)	(1 << x)

#define	TIMER_IRQ_STA_REG 	0x04
#define	 TIMER_IRQ_PENDING(x)	(1 << x)

/*
 * On A10, A13, A20 and A31/A31s 6 timers are available
 */
#define	TIMER_CTRL_REG(x)		(0x10 + 0x10 * x)
#define	 TIMER_CTRL_START		(1 << 0)
#define	 TIMER_CTRL_AUTORELOAD		(1 << 1)
#define	 TIMER_CTRL_CLKSRC_MASK		(3 << 2)
#define	 TIMER_CTRL_OSC24M		(1 << 2)
#define	 TIMER_CTRL_PRESCALAR_MASK	(0x7 << 4)
#define	 TIMER_CTRL_PRESCALAR(x)	((x - 1) << 4)
#define	 TIMER_CTRL_MODE_MASK		(1 << 7)
#define	 TIMER_CTRL_MODE_SINGLE		(1 << 7)
#define	 TIMER_CTRL_MODE_CONTINUOUS	(0 << 7)
#define	TIMER_INTV_REG(x)		(0x14 + 0x10 * x)
#define	TIMER_CURV_REG(x)		(0x18 + 0x10 * x)

/* 64 bit counter, available in A10 and A13 */
#define	CNT64_CTRL_REG		0xa0
#define	 CNT64_CTRL_RL_EN	0x02 /* read latch enable */
#define	CNT64_LO_REG		0xa4
#define	CNT64_HI_REG		0xa8

#define	SYS_TIMER_CLKSRC	24000000 /* clock source */

enum aw_timer_type {
	A10_TIMER = 1,
	A23_TIMER,
	D1_TIMER,
};

struct aw_timer_softc {
	device_t 	sc_dev;
	struct resource *res[2];
	void 		*sc_ih;		/* interrupt handler */
	uint32_t 	sc_period;
	uint64_t 	timer0_freq;
	struct eventtimer	et;
	enum aw_timer_type	type;
};

#define timer_read_4(sc, reg)	\
	bus_read_4(sc->res[AW_TIMER_MEMRES], reg)
#define timer_write_4(sc, reg, val)	\
	bus_write_4(sc->res[AW_TIMER_MEMRES], reg, val)

#if defined(__arm__)
static u_int	a10_timer_get_timecount(struct timecounter *);
static uint64_t	a10_timer_read_counter64(struct aw_timer_softc *sc);
static void	a10_timer_timecounter_setup(struct aw_timer_softc *sc);
#endif

#if defined(__arm__) || defined(__riscv)
#define	USE_EVENTTIMER
static void	aw_timer_eventtimer_setup(struct aw_timer_softc *sc);
static int	aw_timer_eventtimer_start(struct eventtimer *, sbintime_t first,
		    sbintime_t period);
static int	aw_timer_eventtimer_stop(struct eventtimer *);
#endif

#if defined(__aarch64__)
static void a23_timer_timecounter_setup(struct aw_timer_softc *sc);
static u_int a23_timer_get_timecount(struct timecounter *tc);
#endif

static int aw_timer_irq(void *);
static int aw_timer_probe(device_t);
static int aw_timer_attach(device_t);

#if defined(__arm__)
#define	AW_TIMER_QUALITY	1000

static delay_func a10_timer_delay;

static struct timecounter a10_timer_timecounter = {
	.tc_name           = "aw_timer timer0",
	.tc_get_timecount  = a10_timer_get_timecount,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = AW_TIMER_QUALITY,
};
#endif

#if defined(__aarch64__)
/* We want it to be selected over the arm generic timecounter */
#define	AW_TIMER_QUALITY	2000

static struct timecounter a23_timer_timecounter = {
	.tc_name           = "aw_timer timer0",
	.tc_get_timecount  = a23_timer_get_timecount,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = AW_TIMER_QUALITY,
};
#endif

#if defined(__riscv)
/* We want it to be selected over the generic RISC-V eventtimer */
#define	AW_TIMER_QUALITY	2000
#endif

#define	AW_TIMER_MEMRES		0
#define	AW_TIMER_IRQRES		1

static struct resource_spec aw_timer_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static struct ofw_compat_data compat_data[] = {
	{"allwinner,sun4i-a10-timer", A10_TIMER},
#if defined(__aarch64__)
	{"allwinner,sun8i-a23-timer", A23_TIMER},
#elif defined(__riscv)
	{"allwinner,sun20i-d1-timer", D1_TIMER},
#endif
	{NULL, 0},
};

static int
aw_timer_probe(device_t dev)
{
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

#if defined(__arm__)
	/* For SoC >= A10 we have the ARM Timecounter/Eventtimer */
	u_int soc_family = allwinner_soc_family();
	if (soc_family != ALLWINNERSOC_SUN4I &&
	    soc_family != ALLWINNERSOC_SUN5I)
		return (ENXIO);
#endif

	device_set_desc(dev, "Allwinner timer");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_timer_attach(device_t dev)
{
	struct aw_timer_softc *sc;
	clk_t clk;
	int err;

	sc = device_get_softc(dev);
	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	if (bus_alloc_resources(dev, aw_timer_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->sc_dev = dev;

	/* Setup and enable the timer interrupt */
	err = bus_setup_intr(dev, sc->res[AW_TIMER_IRQRES], INTR_TYPE_CLK,
	    aw_timer_irq, NULL, sc, &sc->sc_ih);
	if (err != 0) {
		bus_release_resources(dev, aw_timer_spec, sc->res);
		device_printf(dev, "Unable to setup the clock irq handler, "
		    "err = %d\n", err);
		return (ENXIO);
	}

	if (clk_get_by_ofw_index(dev, 0, 0, &clk) != 0) {
		sc->timer0_freq = SYS_TIMER_CLKSRC;
	} else {
		if (clk_get_freq(clk, &sc->timer0_freq) != 0) {
			device_printf(dev, "Cannot get clock source frequency\n");
			return (ENXIO);
		}
	}

	if (bootverbose) {
		device_printf(sc->sc_dev, "clock: hz=%d stathz = %d\n", hz,
		    stathz);
	}

	/* Set up eventtimer (if applicable) */
#if defined(USE_EVENTTIMER)
	aw_timer_eventtimer_setup(sc);
#endif

	/* Set up timercounter (if applicable) */
#if defined(__arm__)
	a10_timer_timecounter_setup(sc);
#elif defined(__aarch64__)
	a23_timer_timecounter_setup(sc);
#endif

	return (0);
}

static int
aw_timer_irq(void *arg)
{
	struct aw_timer_softc *sc;
	uint32_t val;

	sc = (struct aw_timer_softc *)arg;

	/* Clear interrupt pending bit. */
	timer_write_4(sc, TIMER_IRQ_STA_REG, TIMER_IRQ_PENDING(0));

	val = timer_read_4(sc, TIMER_CTRL_REG(0));

	/*
	 * Disabled autoreload and sc_period > 0 means 
	 * timer_start was called with non NULL first value.
	 * Now we will set periodic timer with the given period 
	 * value.
	 */
	if ((val & (1<<1)) == 0 && sc->sc_period > 0) {
		/* Update timer */
		timer_write_4(sc, TIMER_CURV_REG(0), sc->sc_period);

		/* Make periodic and enable */
		val |= TIMER_CTRL_AUTORELOAD | TIMER_CTRL_START;
		timer_write_4(sc, TIMER_CTRL_REG(0), val);
	}

	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}

/*
 * Event timer function for A10, A13, and D1.
 */
#if defined(USE_EVENTTIMER)
static void
aw_timer_eventtimer_setup(struct aw_timer_softc *sc)
{
	uint32_t val;

	/* Set clock source to OSC24M, 1 pre-division, continuous mode */
	val = timer_read_4(sc, TIMER_CTRL_REG(0));
	val &= ~TIMER_CTRL_PRESCALAR_MASK | ~TIMER_CTRL_MODE_MASK | ~TIMER_CTRL_CLKSRC_MASK;
	val |= TIMER_CTRL_PRESCALAR(1) | TIMER_CTRL_OSC24M;
	timer_write_4(sc, TIMER_CTRL_REG(0), val);

	/* Enable timer0 */
	val = timer_read_4(sc, TIMER_IRQ_EN_REG);
	val |= TIMER_IRQ_ENABLE(0);
	timer_write_4(sc, TIMER_IRQ_EN_REG, val);

	/* Set desired frequency in event timer and timecounter */
	sc->et.et_frequency = sc->timer0_freq;
	sc->et.et_name = "aw_timer Eventtimer";
	sc->et.et_flags = ET_FLAGS_ONESHOT | ET_FLAGS_PERIODIC;
	sc->et.et_quality = AW_TIMER_QUALITY;
	sc->et.et_min_period = (0x00000005LLU << 32) / sc->et.et_frequency;
	sc->et.et_max_period = (0xfffffffeLLU << 32) / sc->et.et_frequency;
	sc->et.et_start = aw_timer_eventtimer_start;
	sc->et.et_stop = aw_timer_eventtimer_stop;
	sc->et.et_priv = sc;
	et_register(&sc->et);

	if (bootverbose) {
		device_printf(sc->sc_dev, "event timer clock frequency %ju\n",
		    sc->timer0_freq);
	}
}

static int
aw_timer_eventtimer_start(struct eventtimer *et, sbintime_t first,
    sbintime_t period)
{
	struct aw_timer_softc *sc;
	uint32_t count;
	uint32_t val;

	sc = (struct aw_timer_softc *)et->et_priv;

	if (period != 0)
		sc->sc_period = ((uint32_t)et->et_frequency * period) >> 32;
	else
		sc->sc_period = 0;
	if (first != 0)
		count = ((uint32_t)et->et_frequency * first) >> 32;
	else
		count = sc->sc_period;

	/* Update timer values */
	timer_write_4(sc, TIMER_INTV_REG(0), sc->sc_period);
	timer_write_4(sc, TIMER_CURV_REG(0), count);

	val = timer_read_4(sc, TIMER_CTRL_REG(0));
	if (period != 0) {
		/* periodic */
		val |= TIMER_CTRL_AUTORELOAD;
	} else {
		/* oneshot */
		val &= ~TIMER_CTRL_AUTORELOAD;
	}
	/* Enable timer0 */
	val |= TIMER_IRQ_ENABLE(0);
	timer_write_4(sc, TIMER_CTRL_REG(0), val);

	return (0);
}

static int
aw_timer_eventtimer_stop(struct eventtimer *et)
{
	struct aw_timer_softc *sc;
	uint32_t val;

	sc = (struct aw_timer_softc *)et->et_priv;

	/* Disable timer0 */
	val = timer_read_4(sc, TIMER_CTRL_REG(0));
	val &= ~TIMER_CTRL_START;
	timer_write_4(sc, TIMER_CTRL_REG(0), val);

	sc->sc_period = 0;

	return (0);
}
#endif /* USE_EVENTTIMER */

/*
 * Timecounter functions for A23 and above
 */

#if defined(__aarch64__)
static void
a23_timer_timecounter_setup(struct aw_timer_softc *sc)
{
	uint32_t val;

	/* Set clock source to OSC24M, 1 pre-division, continuous mode */
	val = timer_read_4(sc, TIMER_CTRL_REG(0));
	val &= ~TIMER_CTRL_PRESCALAR_MASK | ~TIMER_CTRL_MODE_MASK | ~TIMER_CTRL_CLKSRC_MASK;
	val |= TIMER_CTRL_PRESCALAR(1) | TIMER_CTRL_OSC24M;
	timer_write_4(sc, TIMER_CTRL_REG(0), val);

	/* Set reload value */
	timer_write_4(sc, TIMER_INTV_REG(0), ~0);
	val = timer_read_4(sc, TIMER_INTV_REG(0));

	/* Enable timer0 */
	val = timer_read_4(sc, TIMER_CTRL_REG(0));
	val |= TIMER_CTRL_AUTORELOAD | TIMER_CTRL_START;
	timer_write_4(sc, TIMER_CTRL_REG(0), val);

	val = timer_read_4(sc, TIMER_CURV_REG(0));

	a23_timer_timecounter.tc_priv = sc;
	a23_timer_timecounter.tc_frequency = sc->timer0_freq;
	tc_init(&a23_timer_timecounter);

	if (bootverbose) {
		device_printf(sc->sc_dev, "timecounter clock frequency %jd\n",
		    a23_timer_timecounter.tc_frequency);
	}
}

static u_int
a23_timer_get_timecount(struct timecounter *tc)
{
	struct aw_timer_softc *sc;
	uint32_t val;

	sc = (struct aw_timer_softc *)tc->tc_priv;
	if (sc == NULL)
		return (0);

	val = timer_read_4(sc, TIMER_CURV_REG(0));
	/* Counter count backwards */
	return (~0u - val);
}
#endif /* __aarch64__ */

/*
 * Timecounter functions for A10 and A13, using the 64 bits counter
 */

#if defined(__arm__)
static uint64_t
a10_timer_read_counter64(struct aw_timer_softc *sc)
{
	uint32_t lo, hi;

	/* Latch counter, wait for it to be ready to read. */
	timer_write_4(sc, CNT64_CTRL_REG, CNT64_CTRL_RL_EN);
	while (timer_read_4(sc, CNT64_CTRL_REG) & CNT64_CTRL_RL_EN)
		continue;

	hi = timer_read_4(sc, CNT64_HI_REG);
	lo = timer_read_4(sc, CNT64_LO_REG);

	return (((uint64_t)hi << 32) | lo);
}

static void
a10_timer_delay(int usec, void *arg)
{
	struct aw_timer_softc *sc = arg;
	uint64_t end, now;

	now = a10_timer_read_counter64(sc);
	end = now + (sc->timer0_freq / 1000000) * (usec + 1);

	while (now < end)
		now = a10_timer_read_counter64(sc);
}

static u_int
a10_timer_get_timecount(struct timecounter *tc)
{
	if (tc->tc_priv == NULL)
		return (0);

	return ((u_int)a10_timer_read_counter64(tc->tc_priv));
}

static void
a10_timer_timecounter_setup(struct aw_timer_softc *sc)
{
	arm_set_delay(a10_timer_delay, sc);
	a10_timer_timecounter.tc_priv = sc;
	a10_timer_timecounter.tc_frequency = sc->timer0_freq;
	tc_init(&a10_timer_timecounter);

	if (bootverbose) {
		device_printf(sc->sc_dev, "timecounter clock frequency %jd\n",
		    a10_timer_timecounter.tc_frequency);
	}
}
#endif /* __arm__ */

static device_method_t aw_timer_methods[] = {
	DEVMETHOD(device_probe,		aw_timer_probe),
	DEVMETHOD(device_attach,	aw_timer_attach),

	DEVMETHOD_END
};

static driver_t aw_timer_driver = {
	"aw_timer",
	aw_timer_methods,
	sizeof(struct aw_timer_softc),
};

EARLY_DRIVER_MODULE(aw_timer, simplebus, aw_timer_driver, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE);
