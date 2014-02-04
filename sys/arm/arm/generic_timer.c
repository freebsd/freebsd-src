/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * Copyright (c) 2013 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Based on mpcore_timer.c developed by Ben Gray <ben.r.gray@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 *      Cortex-A15 (and probably A7) Generic Timer
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

#define	GT_CTRL_ENABLE		(1 << 0)
#define	GT_CTRL_INT_MASK	(1 << 1)
#define	GT_CTRL_INT_STAT	(1 << 2)
#define	GT_REG_CTRL		0
#define	GT_REG_TVAL		1

#define	GT_CNTKCTL_PL0PTEN	(1 << 9) /* PL0 Physical timer reg access */
#define	GT_CNTKCTL_PL0VTEN	(1 << 8) /* PL0 Virtual timer reg access */
#define	GT_CNTKCTL_EVNTI	(1 << 4) /* Virtual counter event bits */
#define	GT_CNTKCTL_EVNTDIR	(1 << 3) /* Virtual counter event transition */
#define	GT_CNTKCTL_EVNTEN	(1 << 2) /* Enables virtual counter events */
#define	GT_CNTKCTL_PL0VCTEN	(1 << 1) /* PL0 CNTVCT and CNTFRQ access */
#define	GT_CNTKCTL_PL0PCTEN	(1 << 0) /* PL0 CNTPCT and CNTFRQ access */

#define	GT_CNTPSIRQ	29

struct arm_tmr_softc {
	struct resource		*irq_res;
	uint32_t		clkfreq;
	struct eventtimer	et;
};

static struct arm_tmr_softc *arm_tmr_sc = NULL;

static timecounter_get_t arm_tmr_get_timecount;

static struct timecounter arm_tmr_timecount = {
	.tc_name           = "ARM MPCore Timecounter",
	.tc_get_timecount  = arm_tmr_get_timecount,
	.tc_poll_pps       = NULL,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = 1000,
};

static inline int
get_freq(void)
{
	uint32_t val;

	__asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r" (val));

	return (val);
}

static inline int
set_freq(uint32_t val)
{

	__asm volatile("mcr p15, 0, %[val], c14, c0, 0" : :
	    [val] "r" (val));
	isb();

	return (val);
}


static inline long
get_cntpct(void)
{
	uint64_t val;

	__asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (val));

	return (val);
}

static inline int
set_ctrl(uint32_t val)
{

	__asm volatile("mcr p15, 0, %[val], c14, c2, 1" : :
	    [val] "r" (val));
	isb();

	return (0);
}

static inline int
set_tval(uint32_t val)
{

	__asm volatile("mcr p15, 0, %[val], c14, c2, 0" : :
	    [val] "r" (val));
	isb();

	return (0);
}

static inline int
get_ctrl(void)
{
	uint32_t val;

	__asm volatile("mrc p15, 0, %0, c14, c2, 1" : "=r" (val));

	return (val);
}

static inline int
get_tval(void)
{
	uint32_t val;

	__asm volatile("mrc p15, 0, %0, c14, c2, 0" : "=r" (val));

	return (val);
}

static inline void
disable_user_access(void)
{
	uint32_t cntkctl;

	__asm volatile("mrc p15, 0, %0, c14, c1, 0" : "=r" (cntkctl));
	cntkctl &= ~(GT_CNTKCTL_PL0PTEN | GT_CNTKCTL_PL0VTEN |
	    GT_CNTKCTL_EVNTEN | GT_CNTKCTL_PL0VCTEN | GT_CNTKCTL_PL0PCTEN);
	__asm volatile("mcr p15, 0, %0, c14, c1, 0" : : "r" (cntkctl));
	isb();
}

static unsigned
arm_tmr_get_timecount(struct timecounter *tc)
{

	return (get_cntpct());
}

static int
arm_tmr_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	struct arm_tmr_softc *sc;
	int counts, ctrl;

	sc = (struct arm_tmr_softc *)et->et_priv;

	if (first != 0) {
		counts = ((uint32_t)et->et_frequency * first) >> 32;
		ctrl = get_ctrl();
		ctrl &= ~GT_CTRL_INT_MASK;
		ctrl |= GT_CTRL_ENABLE;
		set_tval(counts);
		set_ctrl(ctrl);
		return (0);
	}

	return (EINVAL);

}

static int
arm_tmr_stop(struct eventtimer *et)
{
	int ctrl;

	ctrl = get_ctrl();
	ctrl &= GT_CTRL_ENABLE;
	set_ctrl(ctrl);

	return (0);
}

static int
arm_tmr_intr(void *arg)
{
	struct arm_tmr_softc *sc;
	int ctrl;

	sc = (struct arm_tmr_softc *)arg;
	ctrl = get_ctrl();
	if (ctrl & GT_CTRL_INT_STAT) {
		ctrl |= GT_CTRL_INT_MASK;
		set_ctrl(ctrl);
	}

	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}

static int
arm_tmr_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "arm,armv7-timer"))
		return (ENXIO);

	device_set_desc(dev, "ARMv7 Generic Timer");
	return (BUS_PROBE_DEFAULT);
}


static int
arm_tmr_attach(device_t dev)
{
	struct arm_tmr_softc *sc;
	phandle_t node;
	pcell_t clock;
	void *ihl;
	int rid;
	int error;

	sc = device_get_softc(dev);
	if (arm_tmr_sc)
		return (ENXIO);

	/* Get the base clock frequency */
	node = ofw_bus_get_node(dev);
	error = OF_getprop(node, "clock-frequency", &clock, sizeof(clock));
	if (error <= 0) {
		device_printf(dev, "missing clock-frequency "
		    "attribute in FDT\n");
		return (ENXIO);
	}
	sc->clkfreq = fdt32_to_cpu(clock);

	rid = 0;
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    GT_CNTPSIRQ, GT_CNTPSIRQ,
	    1, RF_SHAREABLE | RF_ACTIVE);

	arm_tmr_sc = sc;

	/* Setup and enable the timer */
	if (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CLK, arm_tmr_intr,
		NULL, sc, &ihl) != 0) {
		bus_release_resource(dev, SYS_RES_IRQ, rid, sc->irq_res);
		device_printf(dev, "Unable to setup the CLK irq handler.\n");
		return (ENXIO);
	}

	set_freq(sc->clkfreq);
	disable_user_access();

	arm_tmr_timecount.tc_frequency = sc->clkfreq;
	tc_init(&arm_tmr_timecount);

	sc->et.et_name = "ARM MPCore Eventtimer";
	sc->et.et_flags = ET_FLAGS_ONESHOT | ET_FLAGS_PERCPU;
	sc->et.et_quality = 1000;

	sc->et.et_frequency = sc->clkfreq;
	sc->et.et_min_period = (0x00000002LLU << 32) / sc->et.et_frequency;
	sc->et.et_max_period = (0xfffffffeLLU << 32) / sc->et.et_frequency;
	sc->et.et_start = arm_tmr_start;
	sc->et.et_stop = arm_tmr_stop;
	sc->et.et_priv = sc;
	et_register(&sc->et);

	return (0);
}

static device_method_t arm_tmr_methods[] = {
	DEVMETHOD(device_probe,		arm_tmr_probe),
	DEVMETHOD(device_attach,	arm_tmr_attach),
	{ 0, 0 }
};

static driver_t arm_tmr_driver = {
	"generic_timer",
	arm_tmr_methods,
	sizeof(struct arm_tmr_softc),
};

static devclass_t arm_tmr_devclass;

DRIVER_MODULE(timer, simplebus, arm_tmr_driver, arm_tmr_devclass, 0, 0);

void
cpu_initclocks(void)
{

	if (PCPU_GET(cpuid) == 0)
		cpu_initclocks_bsp();
	else
		cpu_initclocks_ap();
}

void
DELAY(int usec)
{
	int32_t counts, counts_per_usec;
	uint32_t first, last;

	/*
	 * Check the timers are setup, if not just
	 * use a for loop for the meantime
	 */
	if (arm_tmr_sc == NULL) {
		for (; usec > 0; usec--)
			for (counts = 200; counts > 0; counts--)
				/*
				 * Prevent gcc from optimizing
				 * out the loop
				 */
				cpufunc_nullop();
		return;
	}

	/* Get the number of times to count */
	counts_per_usec = ((arm_tmr_timecount.tc_frequency / 1000000) + 1);

	/*
	 * Clamp the timeout at a maximum value (about 32 seconds with
	 * a 66MHz clock). *Nobody* should be delay()ing for anywhere
	 * near that length of time and if they are, they should be hung
	 * out to dry.
	 */
	if (usec >= (0x80000000U / counts_per_usec))
		counts = (0x80000000U / counts_per_usec) - 1;
	else
		counts = usec * counts_per_usec;

	first = get_cntpct();

	while (counts > 0) {
		last = get_cntpct();
		counts -= (int32_t)(last - first);
		first = last;
	}
}
