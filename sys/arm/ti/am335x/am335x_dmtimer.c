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

#define	AM335X_NUM_TIMERS	8

#define	DMT_TIDR		0x00		/* Identification Register */
#define	DMT_TIOCP_CFG		0x10		/* OCP Configuration Reg */
#define	  DMT_TIOCP_RESET	  (1 << 0)	/* TIOCP perform soft reset */
#define	DMT_IQR_EOI		0x20		/* IRQ End-Of-Interrupt Reg */
#define	DMT_IRQSTATUS_RAW	0x24		/* IRQSTATUS Raw Reg */
#define	DMT_IRQSTATUS		0x28		/* IRQSTATUS Reg */
#define	DMT_IRQENABLE_SET	0x2c		/* IRQSTATUS Set Reg */
#define	DMT_IRQENABLE_CLR	0x30		/* IRQSTATUS Clear Reg */
#define	DMT_IRQWAKEEN		0x34		/* IRQ Wakeup Enable Reg */
#define	  DMT_IRQ_TCAR		  (1 << 0)	/* IRQ: Capture */
#define	  DMT_IRQ_OVF		  (1 << 1)	/* IRQ: Overflow */
#define	  DMT_IRQ_MAT		  (1 << 2)	/* IRQ: Match */
#define	  DMT_IRQ_MASK		  (DMT_IRQ_TCAR | DMT_IRQ_OVF | DMT_IRQ_MAT)
#define	DMT_TCLR		0x38		/* Control Register */
#define	  DMT_TCLR_START	  (1 << 0)	/* Start timer */
#define	  DMT_TCLR_AUTOLOAD	  (1 << 1)	/* Auto-reload on overflow */
#define	  DMT_TCLR_PRES_MASK	  (7 << 2)	/* Prescaler mask */
#define	  DMT_TCLR_PRES_ENABLE	  (1 << 5)	/* Prescaler enable */
#define	  DMT_TCLR_COMP_ENABLE	  (1 << 6)	/* Compare enable */
#define	  DMT_TCLR_PWM_HIGH	  (1 << 7)	/* PWM default output high */
#define	  DMT_TCLR_CAPTRAN_MASK	  (3 << 8)	/* Capture transition mask */
#define	  DMT_TCLR_CAPTRAN_NONE	  (0 << 8)	/* Capture: none */
#define	  DMT_TCLR_CAPTRAN_LOHI	  (1 << 8)	/* Capture lo->hi transition */
#define	  DMT_TCLR_CAPTRAN_HILO	  (2 << 8)	/* Capture hi->lo transition */
#define	  DMT_TCLR_CAPTRAN_BOTH	  (3 << 8)	/* Capture both transitions */
#define	  DMT_TCLR_TRGMODE_MASK	  (3 << 10)	/* Trigger output mode mask */
#define	  DMT_TCLR_TRGMODE_NONE	  (0 << 10)	/* Trigger off */
#define	  DMT_TCLR_TRGMODE_OVFL	  (1 << 10)	/* Trigger on overflow */
#define	  DMT_TCLR_TRGMODE_BOTH	  (2 << 10)	/* Trigger on match + ovflow */
#define	  DMT_TCLR_PWM_PTOGGLE	  (1 << 12)	/* PWM toggles */
#define	  DMT_TCLR_CAP_MODE_2ND	  (1 << 13)	/* Capture second event mode */
#define	  DMT_TCLR_GPO_CFG	  (1 << 14)	/* (no descr in datasheet) */
#define	DMT_TCRR		0x3C		/* Counter Register */
#define	DMT_TLDR		0x40		/* Load Reg */
#define	DMT_TTGR		0x44		/* Trigger Reg */
#define	DMT_TWPS		0x48		/* Write Posted Status Reg */
#define	DMT_TMAR		0x4C		/* Match Reg */
#define	DMT_TCAR1		0x50		/* Capture Reg */
#define	DMT_TSICR		0x54		/* Synchr. Interface Ctrl Reg */
#define	  DMT_TSICR_RESET	0x02		/* TSICR perform soft reset */
#define	DMT_TCAR2		0x48		/* Capture Reg */

struct am335x_dmtimer_softc {
	struct resource *	tmr_mem_res[AM335X_NUM_TIMERS];
	struct resource *	tmr_irq_res[AM335X_NUM_TIMERS];
	uint32_t		sysclk_freq;
	uint32_t		tc_num;		/* Which timer number is tc. */
	uint32_t		et_num;		/* Which timer number is et. */
	struct resource *	tc_memres;	/* Resources for tc timer. */
	struct resource *	et_memres;	/* Resources for et timer. */
	struct timecounter	tc;
	struct eventtimer	et;
};

static struct am335x_dmtimer_softc *am335x_dmtimer_sc;

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

static inline uint32_t
am335x_dmtimer_tc_read_4(struct am335x_dmtimer_softc *sc, uint32_t reg)
{

	return (bus_read_4(sc->tc_memres, reg));
}

static inline void
am335x_dmtimer_tc_write_4(struct am335x_dmtimer_softc *sc, uint32_t reg,
    uint32_t val)
{

	bus_write_4(sc->tc_memres, reg, val);
}

static inline uint32_t
am335x_dmtimer_et_read_4(struct am335x_dmtimer_softc *sc, uint32_t reg)
{

	return (bus_read_4(sc->et_memres, reg));
}

static inline void
am335x_dmtimer_et_write_4(struct am335x_dmtimer_softc *sc, uint32_t reg,
    uint32_t val)
{

	bus_write_4(sc->et_memres, reg, val);
}

static unsigned
am335x_dmtimer_tc_get_timecount(struct timecounter *tc)
{
	struct am335x_dmtimer_softc *sc;

	sc = tc->tc_priv;

	return (am335x_dmtimer_tc_read_4(sc, DMT_TCRR));
}

static int
am335x_dmtimer_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	struct am335x_dmtimer_softc *sc;
	uint32_t count, load, tclr;

	sc = et->et_priv;

	tclr = 0;
	if (period != 0) {
		load = ((uint32_t)et->et_frequency * period) >> 32;
		tclr |= DMT_TCLR_AUTOLOAD;
		panic("periodic timer not implemented\n");
	} else {
		load = 0;
	}

	if (first != 0)
		count = ((uint32_t)et->et_frequency * first) >> 32;
	else
		count = load;

	/* Reset Timer */
	am335x_dmtimer_et_write_4(sc, DMT_TSICR, DMT_TSICR_RESET);

	/* Wait for reset to complete */
	while (am335x_dmtimer_et_read_4(sc, DMT_TIOCP_CFG) & DMT_TIOCP_RESET)
		continue;

	/* set load value */
	am335x_dmtimer_et_write_4(sc, DMT_TLDR, 0xFFFFFFFE - load);

	/* set counter value */
	am335x_dmtimer_et_write_4(sc, DMT_TCRR, 0xFFFFFFFE - count);

	/* enable overflow interrupt */
	am335x_dmtimer_et_write_4(sc, DMT_IRQENABLE_SET, DMT_IRQ_OVF);

	/* start timer(ST) */
	tclr |= DMT_TCLR_START;
	am335x_dmtimer_et_write_4(sc, DMT_TCLR, tclr);

	return (0);
}

static int
am335x_dmtimer_stop(struct eventtimer *et)
{
	struct am335x_dmtimer_softc *sc;

	sc = et->et_priv;

	/* Disable all interrupts */
	am335x_dmtimer_et_write_4(sc, DMT_IRQENABLE_CLR, DMT_IRQ_MASK);

	/* Stop Timer */
	am335x_dmtimer_et_write_4(sc, DMT_TCLR, 0);

	return (0);
}

static int
am335x_dmtimer_intr(void *arg)
{
	struct am335x_dmtimer_softc *sc;

	sc = arg;
	/* Ack interrupt */
	am335x_dmtimer_et_write_4(sc, DMT_IRQSTATUS, DMT_IRQ_OVF);
	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}

static int
am335x_dmtimer_probe(device_t dev)
{

	if (ofw_bus_is_compatible(dev, "ti,am335x-dmtimer")) {
		device_set_desc(dev, "AM335x DMTimer");
		return(BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
am335x_dmtimer_attach(device_t dev)
{
	struct am335x_dmtimer_softc *sc;
	void *ihl;
	int err;

	/*
	 * Note that if this routine returns an error status rather than running
	 * to completion it makes no attempt to clean up allocated resources;
	 * the system is essentially dead anyway without functional timers.
	 */

	sc = device_get_softc(dev);

	if (am335x_dmtimer_sc != NULL)
		return (EINVAL);

	/* Get the base clock frequency. */
	err = ti_prcm_clk_get_source_freq(SYS_CLK, &sc->sysclk_freq);
	if (err) {
		device_printf(dev, "Error: could not get sysclk frequency\n");
		return (ENXIO);
	}

	/* Request the memory resources. */
	err = bus_alloc_resources(dev, am335x_dmtimer_mem_spec,
		sc->tmr_mem_res);
	if (err) {
		device_printf(dev, "Error: could not allocate mem resources\n");
		return (ENXIO);
	}

	/* Request the IRQ resources. */
	err = bus_alloc_resources(dev, am335x_dmtimer_irq_spec,
		sc->tmr_irq_res);
	if (err) {
		device_printf(dev, "Error: could not allocate irq resources\n");
		return (ENXIO);
	}

	/* Configure DMTimer3 as eventtimer and DMTimer4 as timecounter.  */
	sc->et_num = 3;
	sc->tc_num = 2;
	sc->et_memres = sc->tmr_mem_res[sc->et_num];
	sc->tc_memres = sc->tmr_mem_res[sc->tc_num];

	err  = ti_prcm_clk_set_source(DMTIMER0_CLK + sc->et_num, SYSCLK_CLK);
	err |= ti_prcm_clk_enable(DMTIMER0_CLK + sc->et_num);
	err |= ti_prcm_clk_set_source(DMTIMER0_CLK + sc->tc_num, SYSCLK_CLK);
	err |= ti_prcm_clk_enable(DMTIMER0_CLK + sc->tc_num);
	if (err) {
		device_printf(dev, "Error: could not setup timer clock\n");
		return (ENXIO);
	}

	/* Set up timecounter; register tc. */
	am335x_dmtimer_tc_write_4(sc, DMT_TSICR, DMT_TSICR_RESET);
	while (am335x_dmtimer_tc_read_4(sc, DMT_TIOCP_CFG) & DMT_TIOCP_RESET)
		continue;

	am335x_dmtimer_tc_write_4(sc, DMT_TLDR, 0);
	am335x_dmtimer_tc_write_4(sc, DMT_TCRR, 0);
	am335x_dmtimer_tc_write_4(sc, DMT_TCLR, 
	    DMT_TCLR_START | DMT_TCLR_AUTOLOAD);

	sc->tc.tc_name           = "AM335x Timecounter";
	sc->tc.tc_get_timecount  = am335x_dmtimer_tc_get_timecount;
	sc->tc.tc_poll_pps       = NULL;
	sc->tc.tc_counter_mask   = ~0u;
	sc->tc.tc_frequency      = sc->sysclk_freq;
	sc->tc.tc_quality        = 1000;
	sc->tc.tc_priv           = sc;
	tc_init(&sc->tc);

        /* Setup eventtimer; register et. */
	if (bus_setup_intr(dev, sc->tmr_irq_res[sc->et_num], INTR_TYPE_CLK,
			am335x_dmtimer_intr, NULL, sc, &ihl) != 0) {
		bus_release_resources(dev, am335x_dmtimer_irq_spec,
			sc->tmr_irq_res);
		device_printf(dev, "Unable to setup the clock irq handler.\n");
		return (ENXIO);
	}

	sc->et.et_name = "AM335x Eventtimer";
	sc->et.et_flags = ET_FLAGS_ONESHOT;
	sc->et.et_quality = 1000;
	sc->et.et_frequency = sc->sysclk_freq;
	sc->et.et_min_period =
	    ((0x00000005LLU << 32) / sc->et.et_frequency);
	sc->et.et_max_period =
	    (0xfffffffeLLU << 32) / sc->et.et_frequency;
	sc->et.et_start = am335x_dmtimer_start;
	sc->et.et_stop = am335x_dmtimer_stop;
	sc->et.et_priv = sc;
	et_register(&sc->et);

	/* Store a pointer to the softc for use in DELAY(). */
	am335x_dmtimer_sc = sc;

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
	struct am335x_dmtimer_softc *sc;
	int32_t counts;
	uint32_t first, last;

	sc = am335x_dmtimer_sc;

	if (sc == NULL) {
		for (; usec > 0; usec--)
			for (counts = 200; counts > 0; counts--)
				/* Prevent gcc from optimizing  out the loop */
				cpufunc_nullop();
		return;
	}

	/* Get the number of times to count */
	counts = (usec + 1) * (sc->sysclk_freq / 1000000);

	first = am335x_dmtimer_tc_read_4(sc, DMT_TCRR);

	while (counts > 0) {
		last = am335x_dmtimer_tc_read_4(sc, DMT_TCRR);
		if (last > first) {
			counts -= (int32_t)(last - first);
		} else {
			counts -= (int32_t)((0xFFFFFFFF - first) + last);
		}
		first = last;
	}
}

