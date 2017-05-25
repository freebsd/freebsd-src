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
#include <sys/kdb.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define INITIAL_TIMECOUNTER	(0xffffffff)
#define MAX_WATCHDOG_TICKS	(0xffffffff)

#if defined(SOC_MV_ARMADAXP) || defined(SOC_MV_ARMADA38X)
#define MV_CLOCK_SRC		25000000	/* Timers' 25MHz mode */
#else
#define MV_CLOCK_SRC		get_tclk()
#endif

#if defined(SOC_MV_ARMADA38X)
#define	WATCHDOG_TIMER	4
#else
#define	WATCHDOG_TIMER	2
#endif

struct mv_wdt_softc {
	struct resource	*	wdt_res;
	struct mtx		wdt_mtx;
};

static struct resource_spec mv_wdt_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static struct ofw_compat_data mv_wdt_compat[] = {
	{"marvell,armada-380-wdt",	true},
	{NULL,				false}
};

static struct mv_wdt_softc *wdt_softc = NULL;
int timers_initialized = 0;

static int mv_wdt_probe(device_t);
static int mv_wdt_attach(device_t);

static uint32_t	mv_get_timer_control(void);
static void mv_set_timer_control(uint32_t);
static void mv_set_timer(uint32_t, uint32_t);

static void mv_watchdog_enable(void);
static void mv_watchdog_disable(void);
static void mv_watchdog_event(void *, unsigned int, int *);

static device_method_t mv_wdt_methods[] = {
	DEVMETHOD(device_probe, mv_wdt_probe),
	DEVMETHOD(device_attach, mv_wdt_attach),

	{ 0, 0 }
};

static driver_t mv_wdt_driver = {
	"wdt",
	mv_wdt_methods,
	sizeof(struct mv_wdt_softc),
};

static devclass_t mv_wdt_devclass;

DRIVER_MODULE(wdt, simplebus, mv_wdt_driver, mv_wdt_devclass, 0, 0);
static int
mv_wdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, mv_wdt_compat)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Marvell Watchdog Timer");
	return (0);
}

static int
mv_wdt_attach(device_t dev)
{
	struct mv_wdt_softc *sc;
	int error;

	if (wdt_softc != NULL)
		return (ENXIO);

	sc = device_get_softc(dev);
	wdt_softc = sc;

	error = bus_alloc_resources(dev, mv_wdt_spec, &sc->wdt_res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	mtx_init(&sc->wdt_mtx, "watchdog", NULL, MTX_DEF);

	mv_watchdog_disable();
	EVENTHANDLER_REGISTER(watchdog_list, mv_watchdog_event, sc, 0);

	return (0);
}

static __inline uint32_t
mv_get_timer_control(void)
{

	return (bus_read_4(wdt_softc->wdt_res, CPU_TIMER_CONTROL));
}

static __inline void
mv_set_timer_control(uint32_t val)
{

	bus_write_4(wdt_softc->wdt_res, CPU_TIMER_CONTROL, val);
}

static __inline void
mv_set_timer(uint32_t timer, uint32_t val)
{

	bus_write_4(wdt_softc->wdt_res, CPU_TIMER0 + timer * 0x8, val);
}

static void
mv_watchdog_enable(void)
{
	uint32_t val, irq_cause;
#if !defined(SOC_MV_ARMADAXP) && !defined(SOC_MV_ARMADA38X)
	uint32_t irq_mask;
#endif

	irq_cause = read_cpu_ctrl(BRIDGE_IRQ_CAUSE);
	irq_cause &= IRQ_TIMER_WD_CLR;
	write_cpu_ctrl(BRIDGE_IRQ_CAUSE, irq_cause);

#if defined(SOC_MV_ARMADAXP) || defined(SOC_MV_ARMADA38X)
	val = read_cpu_mp_clocks(WD_RSTOUTn_MASK);
	val |= (WD_GLOBAL_MASK | WD_CPU0_MASK);
	write_cpu_mp_clocks(WD_RSTOUTn_MASK, val);

	val = read_cpu_misc(RSTOUTn_MASK);
	val &= ~RSTOUTn_MASK_WD;
	write_cpu_misc(RSTOUTn_MASK, val);
#else
	irq_mask = read_cpu_ctrl(BRIDGE_IRQ_MASK);
	irq_mask |= IRQ_TIMER_WD_MASK;
	write_cpu_ctrl(BRIDGE_IRQ_MASK, irq_mask);

	val = read_cpu_ctrl(RSTOUTn_MASK);
	val |= WD_RST_OUT_EN;
	write_cpu_ctrl(RSTOUTn_MASK, val);
#endif

	val = mv_get_timer_control();
#if defined(SOC_MV_ARMADA38X)
	val |= CPU_TIMER_WD_EN | CPU_TIMER_WD_AUTO | CPU_TIMER_WD_25MHZ_EN;
#elif defined(SOC_MV_ARMADAXP)
	val |= CPU_TIMER2_EN | CPU_TIMER2_AUTO | CPU_TIMER_WD_25MHZ_EN;
#else
	val |= CPU_TIMER2_EN | CPU_TIMER2_AUTO;
#endif
	mv_set_timer_control(val);
}

static void
mv_watchdog_disable(void)
{
	uint32_t val, irq_cause;
#if !defined(SOC_MV_ARMADAXP) && !defined(SOC_MV_ARMADA38X)
	uint32_t irq_mask;
#endif

	val = mv_get_timer_control();
#if defined(SOC_MV_ARMADA38X)
	val &= ~(CPU_TIMER_WD_EN | CPU_TIMER_WD_AUTO);
#else
	val &= ~(CPU_TIMER2_EN | CPU_TIMER2_AUTO);
#endif
	mv_set_timer_control(val);

#if defined(SOC_MV_ARMADAXP) || defined(SOC_MV_ARMADA38X)
	val = read_cpu_mp_clocks(WD_RSTOUTn_MASK);
	val &= ~(WD_GLOBAL_MASK | WD_CPU0_MASK);
	write_cpu_mp_clocks(WD_RSTOUTn_MASK, val);

	val = read_cpu_misc(RSTOUTn_MASK);
	val |= RSTOUTn_MASK_WD;
	write_cpu_misc(RSTOUTn_MASK, RSTOUTn_MASK_WD);
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
	struct mv_wdt_softc *sc;
	uint64_t ns;
	uint64_t ticks;

	sc = arg;
	mtx_lock(&sc->wdt_mtx);
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
			mv_set_timer(WATCHDOG_TIMER, ticks);
			mv_watchdog_enable();
			*error = 0;
		}
	}
	mtx_unlock(&sc->wdt_mtx);
}
