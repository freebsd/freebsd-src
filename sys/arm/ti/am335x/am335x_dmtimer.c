/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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

#include <arm/ti/ti_prcm.h>

#define AM335X_NUM_TIMERS	8

#define DMTIMER_TIDR		0x00 /* Identification Register */
#define DMTIMER_TIOCP_CFG	0x10 /* Timer OCP Configuration Reg */
#define DMTIMER_IQR_EOI		0x20 /* Timer IRQ End-Of-Interrupt Reg */
#define DMTIMER_IRQSTATUS_RAW	0x24 /* Timer IRQSTATUS Raw Reg */
#define DMTIMER_IRQSTATUS	0x28 /* Timer IRQSTATUS Reg */
#define DMTIMER_IRQENABLE_SET	0x2c /* Timer IRQSTATUS Set Reg */
#define DMTIMER_IRQENABLE_CLR	0x30 /* Timer IRQSTATUS Clear Reg */
#define DMTIMER_IRQWAKEEN	0x34 /* Timer IRQ Wakeup Enable Reg */
#define DMTIMER_TCLR		0x38 /* Timer Control Register */
#define DMTIMER_TCRR		0x3C /* Timer Counter Register */
#define DMTIMER_TLDR		0x40 /* Timer Load Reg */
#define DMTIMER_TTGR		0x44 /* Timer Trigger Reg */
#define DMTIMER_TWPS		0x48 /* Timer Write Posted Status Reg */
#define DMTIMER_TMAR		0x4C /* Timer Match Reg */
#define DMTIMER_TCAR1		0x50 /* Timer Capture Reg */
#define DMTIMER_TSICR		0x54 /* Timer Synchr. Interface Control Reg */
#define DMTIMER_TCAR2		0x48 /* Timer Capture Reg */
 

struct am335x_dmtimer_softc {
	struct resource *	tmr_mem_res[AM335X_NUM_TIMERS];
	struct resource *	tmr_irq_res[AM335X_NUM_TIMERS];
	uint32_t		sysclk_freq;
	struct am335x_dmtimer {
		bus_space_tag_t		bst;
		bus_space_handle_t	bsh;
		struct eventtimer	et;
	} t[AM335X_NUM_TIMERS];
};

static struct resource_spec am335x_dmtimer_mem_spec[] = {
	{ SYS_RES_MEMORY,   0,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   1,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   2,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   3,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   4,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   5,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   6,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   7,  RF_ACTIVE },
	{ -1,               0,  0 }
};
static struct resource_spec am335x_dmtimer_irq_spec[] = {
	{ SYS_RES_IRQ,      0,  RF_ACTIVE },
	{ SYS_RES_IRQ,      1,  RF_ACTIVE },
	{ SYS_RES_IRQ,      2,  RF_ACTIVE },
	{ SYS_RES_IRQ,      3,  RF_ACTIVE },
	{ SYS_RES_IRQ,      4,  RF_ACTIVE },
	{ SYS_RES_IRQ,      5,  RF_ACTIVE },
	{ SYS_RES_IRQ,      6,  RF_ACTIVE },
	{ SYS_RES_IRQ,      7,  RF_ACTIVE },
	{ -1,               0,  0 }
};

static struct am335x_dmtimer *am335x_dmtimer_tc_tmr = NULL;

/* Read/Write macros for Timer used as timecounter */
#define am335x_dmtimer_tc_read_4(reg)		\
	bus_space_read_4(am335x_dmtimer_tc_tmr->bst, \
		am335x_dmtimer_tc_tmr->bsh, reg)

#define am335x_dmtimer_tc_write_4(reg, val)	\
	bus_space_write_4(am335x_dmtimer_tc_tmr->bst, \
		am335x_dmtimer_tc_tmr->bsh, reg, val)

/* Read/Write macros for Timer used as eventtimer */
#define am335x_dmtimer_et_read_4(reg)		\
	bus_space_read_4(tmr->bst, tmr->bsh, reg)

#define am335x_dmtimer_et_write_4(reg, val)	\
	bus_space_write_4(tmr->bst, tmr->bsh, reg, val)

static unsigned am335x_dmtimer_tc_get_timecount(struct timecounter *);

static struct timecounter am335x_dmtimer_tc = {
	.tc_name           = "AM335x Timecounter",
	.tc_get_timecount  = am335x_dmtimer_tc_get_timecount,
	.tc_poll_pps       = NULL,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = 1000,
};

static unsigned
am335x_dmtimer_tc_get_timecount(struct timecounter *tc)
{
	return am335x_dmtimer_tc_read_4(DMTIMER_TCRR);
}

static int
am335x_dmtimer_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	struct am335x_dmtimer *tmr = (struct am335x_dmtimer *)et->et_priv;
	uint32_t load, count;
	uint32_t tclr = 0;

	if (period != 0) {
		load = ((uint32_t)et->et_frequency * period) >> 32;
		tclr |= 2; /* autoreload bit */
		panic("periodic timer not implemented\n");
	} else {
		load = 0;
	}

	if (first != 0)
		count = ((uint32_t)et->et_frequency * first) >> 32;
	else
		count = load;

	/* Reset Timer */
	am335x_dmtimer_et_write_4(DMTIMER_TSICR, 2);

	/* Wait for reset to complete */
	while (am335x_dmtimer_et_read_4(DMTIMER_TIOCP_CFG) & 1);

	/* set load value */
	am335x_dmtimer_et_write_4(DMTIMER_TLDR, 0xFFFFFFFE - load);

	/* set counter value */
	am335x_dmtimer_et_write_4(DMTIMER_TCRR, 0xFFFFFFFE - count);

	/* enable overflow interrupt */
	am335x_dmtimer_et_write_4(DMTIMER_IRQENABLE_SET, 2);

	/* start timer(ST) */
	tclr |= 1;
	am335x_dmtimer_et_write_4(DMTIMER_TCLR, tclr);

	return (0);
}

static int
am335x_dmtimer_stop(struct eventtimer *et)
{
	struct am335x_dmtimer *tmr = (struct am335x_dmtimer *)et->et_priv;

	/* Disable all interrupts */
	am335x_dmtimer_et_write_4(DMTIMER_IRQENABLE_CLR, 7);

	/* Stop Timer */
	am335x_dmtimer_et_write_4(DMTIMER_TCLR, 0);

	return (0);
}

static int
am335x_dmtimer_intr(void *arg)
{
	struct am335x_dmtimer *tmr = (struct am335x_dmtimer *)arg;

	/* Ack interrupt */
	am335x_dmtimer_et_write_4(DMTIMER_IRQSTATUS, 7);
	if (tmr->et.et_active)
		tmr->et.et_event_cb(&tmr->et, tmr->et.et_arg);

	return (FILTER_HANDLED);
}

static int
am335x_dmtimer_probe(device_t dev)
{
	struct	am335x_dmtimer_softc *sc;
	sc = (struct am335x_dmtimer_softc *)device_get_softc(dev);

	if (ofw_bus_is_compatible(dev, "ti,am335x-dmtimer")) {
		device_set_desc(dev, "AM335x DMTimer");
		return(BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
am335x_dmtimer_attach(device_t dev)
{
	struct am335x_dmtimer_softc *sc = device_get_softc(dev);
	void *ihl;
	int err;
	int i;

	if (am335x_dmtimer_tc_tmr != NULL)
		return (EINVAL);

	/* Get the base clock frequency */
	err = ti_prcm_clk_get_source_freq(SYS_CLK, &sc->sysclk_freq);
	if (err) {
		device_printf(dev, "Error: could not get sysclk frequency\n");
		return (ENXIO);
	}

	/* Request the memory resources */
	err = bus_alloc_resources(dev, am335x_dmtimer_mem_spec,
		sc->tmr_mem_res);
	if (err) {
		device_printf(dev, "Error: could not allocate mem resources\n");
		return (ENXIO);
	}

	/* Request the IRQ resources */
	err = bus_alloc_resources(dev, am335x_dmtimer_irq_spec,
		sc->tmr_irq_res);
	if (err) {
		device_printf(dev, "Error: could not allocate irq resources\n");
		return (ENXIO);
	}

	for(i=0;i<AM335X_NUM_TIMERS;i++) {
		sc->t[i].bst = rman_get_bustag(sc->tmr_mem_res[i]);
		sc->t[i].bsh = rman_get_bushandle(sc->tmr_mem_res[i]);
	}

	/* Configure DMTimer2 and DMTimer3 source and enable them */
	err  = ti_prcm_clk_set_source(DMTIMER2_CLK, SYSCLK_CLK);
	err |= ti_prcm_clk_enable(DMTIMER2_CLK);
	err |= ti_prcm_clk_set_source(DMTIMER3_CLK, SYSCLK_CLK);
	err |= ti_prcm_clk_enable(DMTIMER3_CLK);
	if (err) {
		device_printf(dev, "Error: could not setup timer clock\n");
		return (ENXIO);
	}

	/* Take DMTimer2 for TC */
	am335x_dmtimer_tc_tmr = &sc->t[2];

	/* Reset Timer */
	am335x_dmtimer_tc_write_4(DMTIMER_TSICR, 2);

	/* Wait for reset to complete */
	while (am335x_dmtimer_tc_read_4(DMTIMER_TIOCP_CFG) & 1);

	/* set load value */
	am335x_dmtimer_tc_write_4(DMTIMER_TLDR, 0);

	/* set counter value */
	am335x_dmtimer_tc_write_4(DMTIMER_TCRR, 0);

	/* Set Timer autoreload(AR) and start timer(ST) */
	am335x_dmtimer_tc_write_4(DMTIMER_TCLR, 3);

	am335x_dmtimer_tc.tc_frequency = sc->sysclk_freq;
	tc_init(&am335x_dmtimer_tc);

	/* Register DMTimer3 as ET */

	/* Setup and enable the timer */
	if (bus_setup_intr(dev, sc->tmr_irq_res[3], INTR_TYPE_CLK,
			am335x_dmtimer_intr, NULL, &sc->t[3], &ihl) != 0) {
		bus_release_resources(dev, am335x_dmtimer_irq_spec,
			sc->tmr_irq_res);
		device_printf(dev, "Unable to setup the clock irq handler.\n");
		return (ENXIO);
	}

	sc->t[3].et.et_name = "AM335x Eventtimer0";
	sc->t[3].et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT;
	sc->t[3].et.et_quality = 1000;
	sc->t[3].et.et_frequency = sc->sysclk_freq;
	sc->t[3].et.et_min_period =
	    (0x00000002LLU << 32) / sc->t[3].et.et_frequency;
	sc->t[3].et.et_max_period =
	    (0xfffffffeLLU << 32) / sc->t[3].et.et_frequency;
	sc->t[3].et.et_start = am335x_dmtimer_start;
	sc->t[3].et.et_stop = am335x_dmtimer_stop;
	sc->t[3].et.et_priv = &sc->t[3];
	et_register(&sc->t[3].et);

	return (0);
}

static device_method_t am335x_dmtimer_methods[] = {
	DEVMETHOD(device_probe,		am335x_dmtimer_probe),
	DEVMETHOD(device_attach,	am335x_dmtimer_attach),
	{ 0, 0 }
};

static driver_t am335x_dmtimer_driver = {
	"am335x_dmtimer",
	am335x_dmtimer_methods,
	sizeof(struct am335x_dmtimer_softc),
};

static devclass_t am335x_dmtimer_devclass;

DRIVER_MODULE(am335x_dmtimer, simplebus, am335x_dmtimer_driver, am335x_dmtimer_devclass, 0, 0);
MODULE_DEPEND(am335x_dmtimer, am335x_prcm, 1, 1, 1);

void
cpu_initclocks(void)
{
	cpu_initclocks_bsp();
}

void
DELAY(int usec)
{
		int32_t counts;
	uint32_t first, last;

	if (am335x_dmtimer_tc_tmr == NULL) {
		for (; usec > 0; usec--)
			for (counts = 200; counts > 0; counts--)
				/* Prevent gcc from optimizing  out the loop */
				cpufunc_nullop();
		return;
	}

	/* Get the number of times to count */
	counts = usec * (am335x_dmtimer_tc.tc_frequency / 1000000) + 1;

	first = am335x_dmtimer_tc_read_4(DMTIMER_TCRR);

	while (counts > 0) {
		last = am335x_dmtimer_tc_read_4(DMTIMER_TCRR);
		if (last>first) {
			counts -= (int32_t)(last - first);
		} else {
			counts -= (int32_t)((0xFFFFFFFF - first) + last);
		}
		first = last;
	}
}

