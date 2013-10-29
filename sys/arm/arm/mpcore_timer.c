/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * Developed by Ben Gray <ben.r.gray@gmail.com>
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
 *	The ARM Cortex-A9 core can support a global timer plus a private and
 *	watchdog timer per core.  This driver reserves memory and interrupt
 *	resources for accessing both timer register sets, these resources are
 *	stored globally and used to setup the timecount and eventtimer.
 *
 *	The timecount timer uses the global 64-bit counter, whereas the
 *	per-CPU eventtimer uses the private 32-bit counters.
 *
 *
 *	REF: ARM Cortex-A9 MPCore, Technical Reference Manual (rev. r2p2)
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

/* Private (per-CPU) timer register map */
#define PRV_TIMER_LOAD                 0x0000
#define PRV_TIMER_COUNT                0x0004
#define PRV_TIMER_CTRL                 0x0008
#define PRV_TIMER_INTR                 0x000C

#define PRV_TIMER_CTR_PRESCALER_SHIFT  8
#define PRV_TIMER_CTRL_IRQ_ENABLE      (1UL << 2)
#define PRV_TIMER_CTRL_AUTO_RELOAD     (1UL << 1)
#define PRV_TIMER_CTRL_TIMER_ENABLE    (1UL << 0)

#define PRV_TIMER_INTR_EVENT           (1UL << 0)

/* Global timer register map */
#define GBL_TIMER_COUNT_LOW            0x0000
#define GBL_TIMER_COUNT_HIGH           0x0004
#define GBL_TIMER_CTRL                 0x0008
#define GBL_TIMER_INTR                 0x000C

#define GBL_TIMER_CTR_PRESCALER_SHIFT  8
#define GBL_TIMER_CTRL_AUTO_INC        (1UL << 3)
#define GBL_TIMER_CTRL_IRQ_ENABLE      (1UL << 2)
#define GBL_TIMER_CTRL_COMP_ENABLE     (1UL << 1)
#define GBL_TIMER_CTRL_TIMER_ENABLE    (1UL << 0)

#define GBL_TIMER_INTR_EVENT           (1UL << 0)

struct arm_tmr_softc {
	struct resource *	tmr_res[4];
	bus_space_tag_t		prv_bst;
	bus_space_tag_t		gbl_bst;
	bus_space_handle_t	prv_bsh;
	bus_space_handle_t	gbl_bsh;
	uint32_t		clkfreq;
	struct eventtimer	et;
};

static struct resource_spec arm_tmr_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },	/* Global registers */
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },    /* Global timer interrupt (unused) */
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },	/* Private (per-CPU) registers */
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },    /* Private timer interrupt */
	{ -1, 0 }
};

static struct arm_tmr_softc *arm_tmr_sc = NULL;

uint32_t platform_arm_tmr_freq = 0;

#define	tmr_prv_read_4(reg)		\
    bus_space_read_4(arm_tmr_sc->prv_bst, arm_tmr_sc->prv_bsh, reg)
#define	tmr_prv_write_4(reg, val)		\
    bus_space_write_4(arm_tmr_sc->prv_bst, arm_tmr_sc->prv_bsh, reg, val)
#define	tmr_gbl_read_4(reg)		\
    bus_space_read_4(arm_tmr_sc->gbl_bst, arm_tmr_sc->gbl_bsh, reg)
#define	tmr_gbl_write_4(reg, val)		\
    bus_space_write_4(arm_tmr_sc->gbl_bst, arm_tmr_sc->gbl_bsh, reg, val)


static timecounter_get_t arm_tmr_get_timecount;

static struct timecounter arm_tmr_timecount = {
	.tc_name           = "ARM MPCore Timecounter",
	.tc_get_timecount  = arm_tmr_get_timecount,
	.tc_poll_pps       = NULL,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = 1000,
};

/**
 *	arm_tmr_get_timecount - reads the timecount (global) timer
 *	@tc: pointer to arm_tmr_timecount struct
 *
 *	We only read the lower 32-bits, the timecount stuff only uses 32-bits
 *	so (for now?) ignore the upper 32-bits.
 *
 *	RETURNS
 *	The lower 32-bits of the counter.
 */
static unsigned
arm_tmr_get_timecount(struct timecounter *tc)
{
	return (tmr_gbl_read_4(GBL_TIMER_COUNT_LOW));
}

/**
 *	arm_tmr_start - starts the eventtimer (private) timer
 *	@et: pointer to eventtimer struct
 *	@first: the number of seconds and fractional sections to trigger in
 *	@period: the period (in seconds and fractional sections) to set
 *
 *	If the eventtimer is required to be in oneshot mode, period will be
 *	NULL and first will point to the time to trigger.  If in periodic mode
 *	period will contain the time period and first may optionally contain
 *	the time for the first period.
 *
 *	RETURNS
 *	Always returns 0
 */
static int
arm_tmr_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	uint32_t load, count;
	uint32_t ctrl;

	ctrl = PRV_TIMER_CTRL_IRQ_ENABLE | PRV_TIMER_CTRL_TIMER_ENABLE;

	if (period != 0) {
		load = ((uint32_t)et->et_frequency * period) >> 32;
		ctrl |= PRV_TIMER_CTRL_AUTO_RELOAD;
	} else
		load = 0;

	if (first != 0)
		count = ((uint32_t)et->et_frequency * first) >> 32;
	else
		count = load;

	tmr_prv_write_4(PRV_TIMER_LOAD, load);
	tmr_prv_write_4(PRV_TIMER_COUNT, count);

	tmr_prv_write_4(PRV_TIMER_CTRL, ctrl);
	return (0);
}

/**
 *	arm_tmr_stop - stops the eventtimer (private) timer
 *	@et: pointer to eventtimer struct
 *
 *	Simply stops the private timer by clearing all bits in the ctrl register.
 *
 *	RETURNS
 *	Always returns 0
 */
static int
arm_tmr_stop(struct eventtimer *et)
{
	tmr_prv_write_4(PRV_TIMER_CTRL, 0);
	return (0);
}

/**
 *	arm_tmr_intr - ISR for the eventtimer (private) timer
 *	@arg: pointer to arm_tmr_softc struct
 *
 *	Clears the event register and then calls the eventtimer callback.
 *
 *	RETURNS
 *	Always returns FILTER_HANDLED
 */
static int
arm_tmr_intr(void *arg)
{
	struct arm_tmr_softc *sc = (struct arm_tmr_softc *)arg;

	tmr_prv_write_4(PRV_TIMER_INTR, PRV_TIMER_INTR_EVENT);

	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}




/**
 *	arm_tmr_probe - timer probe routine
 *	@dev: new device
 *
 *	The probe function returns success when probed with the fdt compatible
 *	string set to "arm,mpcore-timers".
 *
 *	RETURNS
 *	BUS_PROBE_DEFAULT if the fdt device is compatible, otherwise ENXIO.
 */
static int
arm_tmr_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "arm,mpcore-timers"))
		return (ENXIO);

	device_set_desc(dev, "ARM Generic MPCore Timers");
	return (BUS_PROBE_DEFAULT);
}

/**
 *	arm_tmr_attach - attaches the timer to the simplebus
 *	@dev: new device
 *
 *	Reserves memory and interrupt resources, stores the softc structure
 *	globally and registers both the timecount and eventtimer objects.
 *
 *	RETURNS
 *	Zero on sucess or ENXIO if an error occuried.
 */
static int
arm_tmr_attach(device_t dev)
{
	struct arm_tmr_softc *sc = device_get_softc(dev);
	phandle_t node;
	pcell_t clock;
	void *ihl;

	if (arm_tmr_sc)
		return (ENXIO);

	if (platform_arm_tmr_freq != 0)
		sc->clkfreq = platform_arm_tmr_freq;
	else {
		/* Get the base clock frequency */
		node = ofw_bus_get_node(dev);
		if ((OF_getprop(node, "clock-frequency", &clock,
		    sizeof(clock))) <= 0) {
			device_printf(dev, "missing clock-frequency attribute in FDT\n");
			return (ENXIO);
		}
		sc->clkfreq = fdt32_to_cpu(clock);
	}


	if (bus_alloc_resources(dev, arm_tmr_spec, sc->tmr_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Global timer interface */
	sc->gbl_bst = rman_get_bustag(sc->tmr_res[0]);
	sc->gbl_bsh = rman_get_bushandle(sc->tmr_res[0]);

	/* Private per-CPU timer interface */
	sc->prv_bst = rman_get_bustag(sc->tmr_res[2]);
	sc->prv_bsh = rman_get_bushandle(sc->tmr_res[2]);

	arm_tmr_sc = sc;

	/* Disable both timers to start off */
	tmr_prv_write_4(PRV_TIMER_CTRL, 0x00000000);
	tmr_gbl_write_4(GBL_TIMER_CTRL, 0x00000000);

	/* Setup and enable the global timer to use as the timecounter */
	tmr_gbl_write_4(GBL_TIMER_CTRL, (0x00 << GBL_TIMER_CTR_PRESCALER_SHIFT) | 
					GBL_TIMER_CTRL_TIMER_ENABLE);

	arm_tmr_timecount.tc_frequency = sc->clkfreq;
	tc_init(&arm_tmr_timecount);

	/* Setup and enable the timer */
	if (bus_setup_intr(dev, sc->tmr_res[3], INTR_TYPE_CLK, arm_tmr_intr,
			NULL, sc, &ihl) != 0) {
		bus_release_resources(dev, arm_tmr_spec, sc->tmr_res);
		device_printf(dev, "Unable to setup the clock irq handler.\n");
		return (ENXIO);
	}

	sc->et.et_name = "ARM MPCore Eventtimer";
	sc->et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT | ET_FLAGS_PERCPU;
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
	"mp_tmr",
	arm_tmr_methods,
	sizeof(struct arm_tmr_softc),
};

static devclass_t arm_tmr_devclass;

DRIVER_MODULE(mp_tmr, simplebus, arm_tmr_driver, arm_tmr_devclass, 0, 0);

/**
 *	cpu_initclocks - called by system to initialise the cpu clocks
 *
 *	This is a boilerplat function, most of the setup has already been done
 *	when the driver was attached.  Therefore this function must only be called
 *	after the driver is attached.
 *
 *	RETURNS
 *	nothing
 */
void
cpu_initclocks(void)
{
	if (PCPU_GET(cpuid) == 0)
		cpu_initclocks_bsp();
	else
		cpu_initclocks_ap();
}

/**
 *	DELAY - Delay for at least usec microseconds.
 *	@usec: number of microseconds to delay by
 *
 *	This function is called all over the kernel and is suppose to provide a
 *	consistent delay.  This function may also be called before the console 
 *	is setup so no printf's can be called here.
 *
 *	RETURNS:
 *	nothing
 */
void
DELAY(int usec)
{
	int32_t counts_per_usec;
	int32_t counts;
	uint32_t first, last;

	/* Check the timers are setup, if not just use a for loop for the meantime */
	if (arm_tmr_sc == NULL) {
		for (; usec > 0; usec--)
			for (counts = 200; counts > 0; counts--)
				cpufunc_nullop();	/* Prevent gcc from optimizing
							 * out the loop
							 */
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

	first = tmr_gbl_read_4(GBL_TIMER_COUNT_LOW);

	while (counts > 0) {
		last = tmr_gbl_read_4(GBL_TIMER_COUNT_LOW);
		counts -= (int32_t)(last - first);
		first = last;
	}
}
