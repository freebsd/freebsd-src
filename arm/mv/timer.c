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
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#define MV_TIMER_TICK	(get_tclk() / hz)
#define INITIAL_TIMECOUNTER	(0xffffffff)
#define MAX_WATCHDOG_TICKS	(0xffffffff)

struct mv_timer_softc {
	struct resource	*	timer_res[2];
	bus_space_tag_t		timer_bst;
	bus_space_handle_t	timer_bsh;
	struct mtx		timer_mtx;
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
static void	mv_setup_timer(void);
static void	mv_setup_timercount(void);

static struct timecounter mv_timer_timecounter = {
	.tc_get_timecount = mv_timer_get_timecount,
	.tc_name = "CPU Timer",
	.tc_frequency = 0,	/* This is assigned on the fly in the init sequence */
	.tc_counter_mask = ~0u,
	.tc_quality = 1000,
};

static int
mv_timer_probe(device_t dev)
{

	device_set_desc(dev, "Marvell CPU Timer");
	return (0);
}

static int
mv_timer_attach(device_t dev)
{
	int	error;
	void	*ihl;
	struct	mv_timer_softc *sc;

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
	    mv_hardclock, NULL, NULL, &ihl) != 0) {
		bus_release_resources(dev, mv_timer_spec, sc->timer_res);
		device_printf(dev, "could not setup hardclock interrupt\n");
		return (ENXIO);
	}

	mv_setup_timercount();
	timers_initialized = 1;

	return (0);
}

static int
mv_hardclock(void *arg)
{
	uint32_t irq_cause;
	struct	trapframe *frame;

	frame = (struct trapframe *)arg;
	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));

	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
	irq_cause &= ~(IRQ_TIMER0);
	write_cpu_ctrl(BRIDGE_IRQ_CAUSE, irq_cause);

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

DRIVER_MODULE(timer, mbus, mv_timer_driver, mv_timer_devclass, 0, 0);

static unsigned
mv_timer_get_timecount(struct timecounter *tc)
{

	return (INITIAL_TIMECOUNTER - mv_get_timer(1));
}

void
cpu_initclocks(void)
{
	uint32_t irq_cause, irq_mask;

	mv_setup_timer();

	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
	irq_cause &= ~(IRQ_TIMER0);
	write_cpu_ctrl(BRIDGE_IRQ_CAUSE, irq_cause);

	irq_mask = read_cpu_ctrl(BRIDGE_IRQ_MASK);
	irq_mask |= IRQ_TIMER0_MASK;
	write_cpu_ctrl(BRIDGE_IRQ_MASK, irq_mask);

	mv_timer_timecounter.tc_frequency = get_tclk();
	tc_init(&mv_timer_timecounter);
}

void
cpu_startprofclock(void)
{

}

void
cpu_stopprofclock(void)
{

}

void
DELAY(int usec)
{
	uint32_t	val, val_temp;
	int32_t		nticks;

	if (!timers_initialized) {
		for (; usec > 0; usec--)
			for (val = 100; val > 0; val--)
				;
		return;
	}

	val = mv_get_timer(1);
	nticks = ((get_tclk() / 1000000 + 1) * usec);

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
	uint32_t val;
	uint32_t irq_cause, irq_mask;

	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
	irq_cause &= ~(IRQ_TIMER_WD);
	write_cpu_ctrl(BRIDGE_IRQ_CAUSE, irq_cause);

	irq_mask = read_cpu_ctrl(BRIDGE_IRQ_MASK);
	irq_mask |= IRQ_TIMER_WD_MASK;
	write_cpu_ctrl(BRIDGE_IRQ_MASK, irq_mask);

	val = read_cpu_ctrl(RSTOUTn_MASK);
	val |= WD_RST_OUT_EN;
	write_cpu_ctrl(RSTOUTn_MASK, val);

	val = mv_get_timer_control();
	val |= CPU_TIMER_WD_EN | CPU_TIMER_WD_AUTO;
	mv_set_timer_control(val);
}

static void
mv_watchdog_disable(void)
{
	uint32_t val;
	uint32_t irq_cause, irq_mask;

	val = mv_get_timer_control();
	val &= ~(CPU_TIMER_WD_EN | CPU_TIMER_WD_AUTO);
	mv_set_timer_control(val);

	val = read_cpu_ctrl(RSTOUTn_MASK);
	val &= ~WD_RST_OUT_EN;
	write_cpu_ctrl(RSTOUTn_MASK, val);

	irq_mask = read_cpu_ctrl(BRIDGE_IRQ_MASK);
	irq_mask &= ~(IRQ_TIMER_WD_MASK);
	write_cpu_ctrl(BRIDGE_IRQ_MASK, irq_mask);

	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
	irq_cause &= ~(IRQ_TIMER_WD);
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
		ticks = (uint64_t)(ns * get_tclk()) / 1000000000;
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

static void
mv_setup_timer(void)
{
	uint32_t val;

	mv_set_timer_rel(0, MV_TIMER_TICK);
	mv_set_timer(0, MV_TIMER_TICK);
	val = mv_get_timer_control();
	val |= CPU_TIMER0_EN | CPU_TIMER0_AUTO;
	mv_set_timer_control(val);
}

static void
mv_setup_timercount(void)
{
	uint32_t val;

	mv_set_timer_rel(1, INITIAL_TIMECOUNTER);
	mv_set_timer(1, INITIAL_TIMECOUNTER);
	val = mv_get_timer_control();
	val |= CPU_TIMER1_EN | CPU_TIMER1_AUTO;
	mv_set_timer_control(val);
}
