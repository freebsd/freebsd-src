/*-
 * Copyright (c) 2006 Benno Rice.
 * Copyright (C) 2007-2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Adapted to Marvell SoC by Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/xscale/pxa2x0/pxa2x0_timer.c, rev 1
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

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define INITIAL_TIMECOUNTER	(0xffffffff)
#define MAX_WATCHDOG_TICKS	(0xffffffff)

#if defined(SOC_MV_ARMADAXP)
#define MV_CLOCK_SRC		25000000	/* Timers' 25MHz mode */
#else
#define MV_CLOCK_SRC		get_tclk()
#endif

struct mv_timer_softc {
	struct resource	*	timer_res[2];
	bus_space_tag_t		timer_bst;
	bus_space_handle_t	timer_bsh;
	struct mtx		timer_mtx;
	struct eventtimer	et;
};

static struct resource_spec mv_timer_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static struct mv_timer_softc *timer_softc = NULL;
static int timers_initialized = 0;

static int	mv_timer_probe(device_t);
static int	mv_timer_attach(device_t);

static int	mv_hardclock(void *);
static unsigned mv_timer_get_timecount(struct timecounter *);

static uint32_t	mv_get_timer_control(void);
static void	mv_set_timer_control(uint32_t);
static uint32_t	mv_get_timer(uint32_t);
static void	mv_set_timer(uint32_t, uint32_t);
static void	mv_set_timer_rel(uint32_t, uint32_t);
static void	mv_watchdog_enable(void);
static void	mv_watchdog_disable(void);
static void	mv_watchdog_event(void *, unsigned int, int *);
static int	mv_timer_start(struct eventtimer *et,
    sbintime_t first, sbintime_t period);
static int	mv_timer_stop(struct eventtimer *et);
static void	mv_setup_timers(void);

static struct timecounter mv_timer_timecounter = {
	.tc_get_timecount = mv_timer_get_timecount,
	.tc_name = "CPUTimer1",
	.tc_frequency = 0,	/* This is assigned on the fly in the init sequence */
	.tc_counter_mask = ~0u,
	.tc_quality = 1000,
};

static int
mv_timer_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "mrvl,timer"))
		return (ENXIO);

	device_set_desc(dev, "Marvell CPU Timer");
	return (0);
}

static int
mv_timer_attach(device_t dev)
{
	int	error;
	void	*ihl;
	struct	mv_timer_softc *sc;
#if !defined(SOC_MV_ARMADAXP)
	uint32_t irq_cause, irq_mask;
#endif

	if (timer_softc != NULL)
		return (ENXIO);

	sc = (struct mv_timer_softc *)device_get_softc(dev);
	timer_softc = sc;

	error = bus_alloc_resources(dev, mv_timer_spec, sc->timer_res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->timer_bst = rman_get_bustag(sc->timer_res[0]);
	sc->timer_bsh = rman_get_bushandle(sc->timer_res[0]);

	mtx_init(&timer_softc->timer_mtx, "watchdog", NULL, MTX_DEF);
	mv_watchdog_disable();
	EVENTHANDLER_REGISTER(watchdog_list, mv_watchdog_event, sc, 0);

	if (bus_setup_intr(dev, sc->timer_res[1], INTR_TYPE_CLK,
	    mv_hardclock, NULL, sc, &ihl) != 0) {
		bus_release_resources(dev, mv_timer_spec, sc->timer_res);
		device_printf(dev, "Could not setup interrupt.\n");
		return (ENXIO);
	}

	mv_setup_timers();
#if !defined(SOC_MV_ARMADAXP)
	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
        irq_cause &= IRQ_TIMER0_CLR;

	write_cpu_ctrl(BRIDGE_IRQ_CAUSE, irq_cause);
	irq_mask = read_cpu_ctrl(BRIDGE_IRQ_MASK);
	irq_mask |= IRQ_TIMER0_MASK;
	irq_mask &= ~IRQ_TIMER1_MASK;
	write_cpu_ctrl(BRIDGE_IRQ_MASK, irq_mask);
#endif
	sc->et.et_name = "CPUTimer0";
	sc->et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT;
	sc->et.et_quality = 1000;

	sc->et.et_frequency = MV_CLOCK_SRC;
	sc->et.et_min_period = (0x00000002LLU << 32) / sc->et.et_frequency;
	sc->et.et_max_period = (0xfffffffeLLU << 32) / sc->et.et_frequency;
	sc->et.et_start = mv_timer_start;
	sc->et.et_stop = mv_timer_stop;
	sc->et.et_priv = sc;
	et_register(&sc->et);
	mv_timer_timecounter.tc_frequency = MV_CLOCK_SRC;
	tc_init(&mv_timer_timecounter);

	return (0);
}

static int
mv_hardclock(void *arg)
{
	struct	mv_timer_softc *sc;
	uint32_t irq_cause;

	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
	irq_cause &= IRQ_TIMER0_CLR;
	write_cpu_ctrl(BRIDGE_IRQ_CAUSE, irq_cause);

	sc = (struct mv_timer_softc *)arg;
	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}

static device_method_t mv_timer_methods[] = {
	DEVMETHOD(device_probe, mv_timer_probe),
	DEVMETHOD(device_attach, mv_timer_attach),

	{ 0, 0 }
};

static driver_t mv_timer_driver = {
	"timer",
	mv_timer_methods,
	sizeof(struct mv_timer_softc),
};

static devclass_t mv_timer_devclass;

DRIVER_MODULE(timer, simplebus, mv_timer_driver, mv_timer_devclass, 0, 0);

static unsigned
mv_timer_get_timecount(struct timecounter *tc)
{

	return (INITIAL_TIMECOUNTER - mv_get_timer(1));
}

void
cpu_initclocks(void)
{

	cpu_initclocks_bsp();
}

void
DELAY(int usec)
{
	uint32_t	val, val_temp;
	int32_t		nticks;

	if (!timers_initialized) {
		for (; usec > 0; usec--)
			for (val = 100; val > 0; val--)
				__asm __volatile("nop" ::: "memory");
		return;
	}

	val = mv_get_timer(1);
	nticks = ((MV_CLOCK_SRC / 1000000 + 1) * usec);

	while (nticks > 0) {
		val_temp = mv_get_timer(1);
		if (val > val_temp)
			nticks -= (val - val_temp);
		else
			nticks -= (val + (INITIAL_TIMECOUNTER - val_temp));

		val = val_temp;
	}
}

static uint32_t
mv_get_timer_control(void)
{

	return (bus_space_read_4(timer_softc->timer_bst,
	    timer_softc->timer_bsh, CPU_TIMER_CONTROL));
}

static void
mv_set_timer_control(uint32_t val)
{

	bus_space_write_4(timer_softc->timer_bst,
	    timer_softc->timer_bsh, CPU_TIMER_CONTROL, val);
}

static uint32_t
mv_get_timer(uint32_t timer)
{

	return (bus_space_read_4(timer_softc->timer_bst,
	    timer_softc->timer_bsh, CPU_TIMER0 + timer * 0x8));
}

static void
mv_set_timer(uint32_t timer, uint32_t val)
{

	bus_space_write_4(timer_softc->timer_bst,
	    timer_softc->timer_bsh, CPU_TIMER0 + timer * 0x8, val);
}

static void
mv_set_timer_rel(uint32_t timer, uint32_t val)
{

	bus_space_write_4(timer_softc->timer_bst,
	    timer_softc->timer_bsh, CPU_TIMER0_REL + timer * 0x8, val);
}

static void
mv_watchdog_enable(void)
{
	uint32_t val, irq_cause;
#if !defined(SOC_MV_ARMADAXP)
	uint32_t irq_mask;
#endif

	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
	irq_cause &= IRQ_TIMER_WD_CLR;
	write_cpu_ctrl(BRIDGE_IRQ_CAUSE, irq_cause);

#if defined(SOC_MV_ARMADAXP)
	val = read_cpu_mp_clocks(WD_RSTOUTn_MASK);
	val |= (WD_GLOBAL_MASK | WD_CPU0_MASK);
	write_cpu_mp_clocks(WD_RSTOUTn_MASK, val);
#else
	irq_mask = read_cpu_ctrl(BRIDGE_IRQ_MASK);
	irq_mask |= IRQ_TIMER_WD_MASK;
	write_cpu_ctrl(BRIDGE_IRQ_MASK, irq_mask);

	val = read_cpu_ctrl(RSTOUTn_MASK);
	val |= WD_RST_OUT_EN;
	write_cpu_ctrl(RSTOUTn_MASK, val);
#endif

	val = mv_get_timer_control();
	val |= CPU_TIMER_WD_EN | CPU_TIMER_WD_AUTO;
#if defined(SOC_MV_ARMADAXP)
	val |= CPU_TIMER_WD_25MHZ_EN;
#endif
	mv_set_timer_control(val);
}

static void
mv_watchdog_disable(void)
{
	uint32_t val, irq_cause;
#if !defined(SOC_MV_ARMADAXP)
	uint32_t irq_mask;
#endif

	val = mv_get_timer_control();
	val &= ~(CPU_TIMER_WD_EN | CPU_TIMER_WD_AUTO);
	mv_set_timer_control(val);

#if defined(SOC_MV_ARMADAXP)
	val = read_cpu_mp_clocks(WD_RSTOUTn_MASK);
	val &= ~(WD_GLOBAL_MASK | WD_CPU0_MASK);
	write_cpu_mp_clocks(WD_RSTOUTn_MASK, val);
#else
	val = read_cpu_ctrl(RSTOUTn_MASK);
	val &= ~WD_RST_OUT_EN;
	write_cpu_ctrl(RSTOUTn_MASK, val);

	irq_mask = read_cpu_ctrl(BRIDGE_IRQ_MASK);
	irq_mask &= ~(IRQ_TIMER_WD_MASK);
	write_cpu_ctrl(BRIDGE_IRQ_MASK, irq_mask);
#endif

	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
	irq_cause &= IRQ_TIMER_WD_CLR;
	write_cpu_ctrl(BRIDGE_IRQ_CAUSE, irq_cause);
}


/*
 * Watchdog event handler.
 */
static void
mv_watchdog_event(void *arg, unsigned int cmd, int *error)
{
	uint64_t ns;
	uint64_t ticks;

	mtx_lock(&timer_softc->timer_mtx);
	if (cmd == 0)
		mv_watchdog_disable();
	else {
		/*
		 * Watchdog timeout is in nanosecs, calculation according to
		 * watchdog(9)
		 */
		ns = (uint64_t)1 << (cmd & WD_INTERVAL);
		ticks = (uint64_t)(ns * MV_CLOCK_SRC) / 1000000000;
		if (ticks > MAX_WATCHDOG_TICKS)
			mv_watchdog_disable();
		else {
			/* Timer 2 is the watchdog */
			mv_set_timer(2, ticks);
			mv_watchdog_enable();
			*error = 0;
		}
	}
	mtx_unlock(&timer_softc->timer_mtx);
}

static int
mv_timer_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	struct	mv_timer_softc *sc;
	uint32_t val, val1;

	/* Calculate dividers. */
	sc = (struct mv_timer_softc *)et->et_priv;
	if (period != 0)
		val = ((uint32_t)sc->et.et_frequency * period) >> 32;
	else
		val = 0;
	if (first != 0)
		val1 = ((uint32_t)sc->et.et_frequency * first) >> 32;
	else
		val1 = val;

	/* Apply configuration. */
	mv_set_timer_rel(0, val);
	mv_set_timer(0, val1);
	val = mv_get_timer_control();
	val |= CPU_TIMER0_EN;
	if (period != 0)
		val |= CPU_TIMER0_AUTO;
	else
		val &= ~CPU_TIMER0_AUTO;
	mv_set_timer_control(val);
	return (0);
}

static int
mv_timer_stop(struct eventtimer *et)
{
	uint32_t val;

	val = mv_get_timer_control();
	val &= ~(CPU_TIMER0_EN | CPU_TIMER0_AUTO);
	mv_set_timer_control(val);
	return (0);
}

static void
mv_setup_timers(void)
{
	uint32_t val;

	mv_set_timer_rel(1, INITIAL_TIMECOUNTER);
	mv_set_timer(1, INITIAL_TIMECOUNTER);
	val = mv_get_timer_control();
	val &= ~(CPU_TIMER0_EN | CPU_TIMER0_AUTO);
	val |= CPU_TIMER1_EN | CPU_TIMER1_AUTO;
#if defined(SOC_MV_ARMADAXP)
	/* Enable 25MHz mode */
	val |= CPU_TIMER0_25MHZ_EN | CPU_TIMER1_25MHZ_EN;
#endif
	mv_set_timer_control(val);
	timers_initialized = 1;
}
