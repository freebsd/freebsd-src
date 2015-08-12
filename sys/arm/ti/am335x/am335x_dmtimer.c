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
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/timeet.h>
#include <sys/timepps.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include "opt_ntp.h"

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_hwmods.h>
#include <arm/ti/ti_pinmux.h>

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
#define	  DMT_IRQ_MAT		  (1 << 0)	/* IRQ: Match */
#define	  DMT_IRQ_OVF		  (1 << 1)	/* IRQ: Overflow */
#define	  DMT_IRQ_TCAR		  (1 << 2)	/* IRQ: Capture */
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
#define	  DMT_TSICR_RESET	  (1 << 1)	/* TSICR perform soft reset */
#define	DMT_TCAR2		0x48		/* Capture Reg */

#define	DMTIMER_READ4(sc, reg)	(bus_read_4((sc)->tmr_mem_res, (reg)))
#define	DMTIMER_WRITE4(sc, reg, val)	(bus_write_4((sc)->tmr_mem_res, (reg), (val)))

struct am335x_dmtimer_softc {
	device_t		dev;
	int			tmr_mem_rid;
	struct resource *	tmr_mem_res;
	int			tmr_irq_rid;
	struct resource *	tmr_irq_res;
	void			*tmr_irq_handler;
	uint32_t		sysclk_freq;
	uint32_t		tclr;		/* Cached TCLR register. */
	int			pps_curmode;	/* Edge mode now set in hw. */
	struct task 		pps_task;	/* For pps_event handling. */
	struct cdev *		pps_cdev;
	struct pps_state 	pps;

	union {
		struct timecounter tc;
		struct eventtimer et;
	} func;
};

static struct am335x_dmtimer_softc *am335x_dmtimer_et_sc = NULL;
static struct am335x_dmtimer_softc *am335x_dmtimer_tc_sc = NULL;


#ifdef PPS_SYNC
/* -1 - not detected, 0 - not found, > 0 - timerX module */
static int am335x_dmtimer_pps_module = -1;
static const char *am335x_dmtimer_pps_hwmod = NULL;
#endif

/*
 * PPS driver routines, included when the kernel is built with option PPS_SYNC.
 *
 * Note that this PPS driver does not use an interrupt.  Instead it uses the
 * hardware's ability to latch the timer's count register in response to a
 * signal on an IO pin.  Each of timers 4-7 have an associated pin, and this
 * code allows any one of those to be used.
 *
 * The timecounter routines in kern_tc.c call the pps poll routine periodically
 * to see if a new counter value has been latched.  When a new value has been
 * latched, the only processing done in the poll routine is to capture the
 * current set of timecounter timehands (done with pps_capture()) and the
 * latched value from the timer.  The remaining work (done by pps_event()) is
 * scheduled to be done later in a non-interrupt context.
 */
#ifdef PPS_SYNC

#define	PPS_CDEV_NAME	"dmtpps"

static void
am335x_dmtimer_set_capture_mode(struct am335x_dmtimer_softc *sc, bool force_off)
{
	int newmode;

	if (force_off)
		newmode = 0;
	else
		newmode = sc->pps.ppsparam.mode & PPS_CAPTUREBOTH;

	if (newmode == sc->pps_curmode)
		return;

	sc->pps_curmode = newmode;
	sc->tclr &= ~DMT_TCLR_CAPTRAN_MASK;
	switch (newmode) {
	case PPS_CAPTUREASSERT:
		sc->tclr |= DMT_TCLR_CAPTRAN_LOHI;
		break;
	case PPS_CAPTURECLEAR:
		sc->tclr |= DMT_TCLR_CAPTRAN_HILO;
		break;
	default:
		/* It can't be BOTH, so it's disabled. */
		break;
	}
	DMTIMER_WRITE4(sc, DMT_TCLR, sc->tclr);
}

static void
am335x_dmtimer_tc_poll_pps(struct timecounter *tc)
{
	struct am335x_dmtimer_softc *sc;

	sc = tc->tc_priv;

	/*
	 * Note that we don't have the TCAR interrupt enabled, but the hardware
	 * still provides the status bits in the "RAW" status register even when
	 * they're masked from generating an irq.  However, when clearing the
	 * TCAR status to re-arm the capture for the next second, we have to
	 * write to the IRQ status register, not the RAW register.  Quirky.
	 */
	if (DMTIMER_READ4(sc, DMT_IRQSTATUS_RAW) & DMT_IRQ_TCAR) {
		pps_capture(&sc->pps);
		sc->pps.capcount = DMTIMER_READ4(sc, DMT_TCAR1);
		DMTIMER_WRITE4(sc, DMT_IRQSTATUS, DMT_IRQ_TCAR);
		taskqueue_enqueue_fast(taskqueue_fast, &sc->pps_task);
	}
}

static void
am335x_dmtimer_process_pps_event(void *arg, int pending)
{
	struct am335x_dmtimer_softc *sc;

	sc = arg;

	/* This is the task function that gets enqueued by poll_pps.  Once the
	 * time has been captured in the hw interrupt context, the remaining
	 * (more expensive) work to process the event is done later in a
	 * non-fast-interrupt context.
	 *
	 * We only support capture of the rising or falling edge, not both at
	 * once; tell the kernel to process whichever mode is currently active.
	 */
	pps_event(&sc->pps, sc->pps.ppsparam.mode & PPS_CAPTUREBOTH);
}

static int
am335x_dmtimer_pps_open(struct cdev *dev, int flags, int fmt, 
    struct thread *td)
{
	struct am335x_dmtimer_softc *sc;

	sc = dev->si_drv1;

	/* Enable capture on open.  Harmless if already open. */
	am335x_dmtimer_set_capture_mode(sc, 0);

	return 0;
}

static	int
am335x_dmtimer_pps_close(struct cdev *dev, int flags, int fmt, 
    struct thread *td)
{
	struct am335x_dmtimer_softc *sc;

	sc = dev->si_drv1;

	/*
	 * Disable capture on last close.  Use the force-off flag to override
	 * the configured mode and turn off the hardware capture.
	 */
	am335x_dmtimer_set_capture_mode(sc, 1);

	return 0;
}

static int
am335x_dmtimer_pps_ioctl(struct cdev *dev, u_long cmd, caddr_t data, 
    int flags, struct thread *td)
{
	struct am335x_dmtimer_softc *sc;
	int err;

	sc = dev->si_drv1;

	/*
	 * The hardware has a "capture both edges" mode, but we can't do
	 * anything useful with it in terms of PPS capture, so don't even try.
	 */
	if ((sc->pps.ppsparam.mode & PPS_CAPTUREBOTH) == PPS_CAPTUREBOTH)
		return (EINVAL);

	/* Let the kernel do the heavy lifting for ioctl. */
	err = pps_ioctl(cmd, data, &sc->pps);
	if (err != 0)
		return (err);

	/*
	 * The capture mode could have changed, set the hardware to whatever
	 * mode is now current.  Effectively a no-op if nothing changed.
	 */
	am335x_dmtimer_set_capture_mode(sc, 0);

	return (err);
}

static struct cdevsw am335x_dmtimer_pps_cdevsw = {
	.d_version =    D_VERSION,
	.d_open =       am335x_dmtimer_pps_open,
	.d_close =      am335x_dmtimer_pps_close,
	.d_ioctl =      am335x_dmtimer_pps_ioctl,
	.d_name =       PPS_CDEV_NAME,
};

static void
am335x_dmtimer_pps_find()
{
	int i;
	unsigned int padstate;
	const char * padmux;
	struct padinfo {
		char * ballname;
		const char * muxname;
		int    timer_num;
	} padinfo[] = {
		{"GPMC_ADVn_ALE", "timer4", 4}, 
		{"GPMC_BEn0_CLE", "timer5", 5},
		{"GPMC_WEn",      "timer6", 6},
		{"GPMC_OEn_REn",  "timer7", 7},
	};

	/*
	 * Figure out which pin the user has set up for pps.  We'll use the
	 * first timer that has an external caputure pin configured as input.
	 *
	 * XXX The hieroglyphic "(padstate & (0x01 << 5)))" checks that the pin
	 * is configured for input.  The right symbolic values aren't exported
	 * yet from ti_scm.h.
	 */
	am335x_dmtimer_pps_module = 0;
	for (i = 0; i < nitems(padinfo) && am335x_dmtimer_pps_module == 0; ++i) {
		if (ti_pinmux_padconf_get(padinfo[i].ballname, &padmux, 
		    &padstate) == 0) {
			if (strcasecmp(padinfo[i].muxname, padmux) == 0 &&
			    (padstate & (0x01 << 5))) {
				am335x_dmtimer_pps_module = padinfo[i].timer_num;
				am335x_dmtimer_pps_hwmod = padinfo[i].muxname;
			}
		}
	}


	if (am335x_dmtimer_pps_module == 0) {
		printf("am335x_dmtimer: No DMTimer found with capture pin "
		    "configured as input; PPS driver disabled.\n");
	}
}

/*
 * Set up the PPS cdev and the the kernel timepps stuff.
 *
 * Note that this routine cannot touch the hardware, because bus space resources
 * are not fully set up yet when this is called.
 */
static void
am335x_dmtimer_pps_init(device_t dev, struct am335x_dmtimer_softc *sc)
{
	int unit;

	if (am335x_dmtimer_pps_module == -1)
		am335x_dmtimer_pps_find();

	/* No PPS input */
	if (am335x_dmtimer_pps_module == 0)
		return;

	/* Not PPS-enabled input */
	if ((am335x_dmtimer_pps_module > 0) &&
	    (!ti_hwmods_contains(dev, am335x_dmtimer_pps_hwmod)))
	 	return;

	/*
	 * Indicate our capabilities (pretty much just capture of either edge).
	 * Have the kernel init its part of the pps_state struct and add its
	 * capabilities.
	 */
	sc->pps.ppscap = PPS_CAPTUREBOTH;
	pps_init(&sc->pps);

	/*
	 * Set up to capture the PPS via timecounter polling, and init the task
	 * that does deferred pps_event() processing after capture.
	 */
	sc->func.tc.tc_poll_pps = am335x_dmtimer_tc_poll_pps;
	TASK_INIT(&sc->pps_task, 0, am335x_dmtimer_process_pps_event, sc);

	/* Create the PPS cdev.  */
	unit = device_get_unit(dev);
	sc->pps_cdev = make_dev(&am335x_dmtimer_pps_cdevsw, unit, 
	    UID_ROOT, GID_WHEEL, 0600, PPS_CDEV_NAME);
	sc->pps_cdev->si_drv1 = sc;

	device_printf(dev, "Using DMTimer%d for PPS device /dev/%s%d\n", 
	    am335x_dmtimer_pps_module, PPS_CDEV_NAME, unit);
}

#endif

/*
 * End of PPS driver code.
 */

static unsigned
am335x_dmtimer_tc_get_timecount(struct timecounter *tc)
{
	struct am335x_dmtimer_softc *sc;

	sc = tc->tc_priv;

	return (DMTIMER_READ4(sc, DMT_TCRR));
}

static int
am335x_dmtimer_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	struct am335x_dmtimer_softc *sc;
	uint32_t initial_count, reload_count;

	sc = et->et_priv;

	/*
	 * Stop the timer before changing it.  This routine will often be called
	 * while the timer is still running, to either lengthen or shorten the
	 * current event time.  We need to ensure the timer doesn't expire while
	 * we're working with it.
	 *
	 * Also clear any pending interrupt status, because it's at least
	 * theoretically possible that we're running in a primary interrupt
	 * context now, and a timer interrupt could be pending even before we
	 * stopped the timer.  The more likely case is that we're being called
	 * from the et_event_cb() routine dispatched from our own handler, but
	 * it's not clear to me that that's the only case possible.
	 */
	sc->tclr &= ~(DMT_TCLR_START | DMT_TCLR_AUTOLOAD);
	DMTIMER_WRITE4(sc, DMT_TCLR, sc->tclr);
	DMTIMER_WRITE4(sc, DMT_IRQSTATUS, DMT_IRQ_OVF);

	if (period != 0) {
		reload_count = ((uint32_t)et->et_frequency * period) >> 32;
		sc->tclr |= DMT_TCLR_AUTOLOAD;
	} else {
		reload_count = 0;
	}

	if (first != 0)
		initial_count = ((uint32_t)et->et_frequency * first) >> 32;
	else
		initial_count = reload_count;

	/*
	 * Set auto-reload and current-count values.  This timer hardware counts
	 * up from the initial/reload value and interrupts on the zero rollover.
	 */
	DMTIMER_WRITE4(sc, DMT_TLDR, 0xFFFFFFFF - reload_count);
	DMTIMER_WRITE4(sc, DMT_TCRR, 0xFFFFFFFF - initial_count);

	/* Enable overflow interrupt, and start the timer. */
	DMTIMER_WRITE4(sc, DMT_IRQENABLE_SET, DMT_IRQ_OVF);
	sc->tclr |= DMT_TCLR_START;
	DMTIMER_WRITE4(sc, DMT_TCLR, sc->tclr);

	return (0);
}

static int
am335x_dmtimer_stop(struct eventtimer *et)
{
	struct am335x_dmtimer_softc *sc;

	sc = et->et_priv;

	/* Stop timer, disable and clear interrupt. */
	sc->tclr &= ~(DMT_TCLR_START | DMT_TCLR_AUTOLOAD);
	DMTIMER_WRITE4(sc, DMT_TCLR, sc->tclr);
	DMTIMER_WRITE4(sc, DMT_IRQENABLE_CLR, DMT_IRQ_OVF);
	DMTIMER_WRITE4(sc, DMT_IRQSTATUS, DMT_IRQ_OVF);
	return (0);
}

static int
am335x_dmtimer_intr(void *arg)
{
	struct am335x_dmtimer_softc *sc;

	sc = arg;

	/* Ack the interrupt, and invoke the callback if it's still enabled. */
	DMTIMER_WRITE4(sc, DMT_IRQSTATUS, DMT_IRQ_OVF);
	if (sc->func.et.et_active)
		sc->func.et.et_event_cb(&sc->func.et, sc->func.et.et_arg);

	return (FILTER_HANDLED);
}

/*
 * Checks if timer is suitable to be system timer
 */
static int
am335x_dmtimer_system_compatible(device_t dev)
{
	phandle_t node;

	node = ofw_bus_get_node(dev);
	if (OF_hasprop(node, "ti,timer-alwon"))
		return (0);

	return (1);
}

static int
am335x_dmtimer_init_et(struct am335x_dmtimer_softc *sc)
{
	if (am335x_dmtimer_et_sc != NULL)
		return (EEXIST);

#ifdef PPS_SYNC
	if ((am335x_dmtimer_pps_module > 0) &&
	    (!ti_hwmods_contains(sc->dev, am335x_dmtimer_pps_hwmod))) {
	    	device_printf(sc->dev, "not PPS enabled\n");
	 	return (ENXIO);
	}
#endif

	/* Setup eventtimer interrupt handler. */
	if (bus_setup_intr(sc->dev, sc->tmr_irq_res, INTR_TYPE_CLK,
			am335x_dmtimer_intr, NULL, sc, &sc->tmr_irq_handler) != 0) {
		device_printf(sc->dev, "Unable to setup the clock irq handler.\n");
		return (ENXIO);
	}

	sc->func.et.et_name = "AM335x Eventtimer";
	sc->func.et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT;
	sc->func.et.et_quality = 1000;
	sc->func.et.et_frequency = sc->sysclk_freq;
	sc->func.et.et_min_period =
	    ((0x00000005LLU << 32) / sc->func.et.et_frequency);
	sc->func.et.et_max_period =
	    (0xfffffffeLLU << 32) / sc->func.et.et_frequency;
	sc->func.et.et_start = am335x_dmtimer_start;
	sc->func.et.et_stop = am335x_dmtimer_stop;
	sc->func.et.et_priv = sc;
	et_register(&sc->func.et);

	am335x_dmtimer_et_sc = sc;

	return (0);
}

static int
am335x_dmtimer_init_tc(struct am335x_dmtimer_softc *sc)
{
	if (am335x_dmtimer_tc_sc != NULL)
		return (EEXIST);

	/* Set up timecounter, start it, register it. */
	DMTIMER_WRITE4(sc, DMT_TSICR, DMT_TSICR_RESET);
	while (DMTIMER_READ4(sc, DMT_TIOCP_CFG) & DMT_TIOCP_RESET)
		continue;

	sc->tclr |= DMT_TCLR_START | DMT_TCLR_AUTOLOAD;
	DMTIMER_WRITE4(sc, DMT_TLDR, 0);
	DMTIMER_WRITE4(sc, DMT_TCRR, 0);
	DMTIMER_WRITE4(sc, DMT_TCLR, sc->tclr);

	sc->func.tc.tc_name           = "AM335x Timecounter";
	sc->func.tc.tc_get_timecount  = am335x_dmtimer_tc_get_timecount;
	sc->func.tc.tc_counter_mask   = ~0u;
	sc->func.tc.tc_frequency      = sc->sysclk_freq;
	sc->func.tc.tc_quality        = 1000;
	sc->func.tc.tc_priv           = sc;
	tc_init(&sc->func.tc);

	am335x_dmtimer_tc_sc = sc;

	return (0);
}

static int
am335x_dmtimer_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "ti,am335x-timer-1ms") ||
	    ofw_bus_is_compatible(dev, "ti,am335x-timer")) {
		device_set_desc(dev, "AM335x DMTimer");
		return(BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
am335x_dmtimer_attach(device_t dev)
{
	struct am335x_dmtimer_softc *sc;
	int err;
	clk_ident_t timer_id;
	int enable;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Get the base clock frequency. */
	err = ti_prcm_clk_get_source_freq(SYS_CLK, &sc->sysclk_freq);
	if (err) {
		device_printf(dev, "Error: could not get sysclk frequency\n");
		return (ENXIO);
	}

	/* Request the memory resources. */
	sc->tmr_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->tmr_mem_rid, RF_ACTIVE);
	if (sc->tmr_mem_res == NULL) {
		device_printf(dev, "Error: could not allocate mem resources\n");
		return (ENXIO);
	}

	/* Request the IRQ resources. */
	sc->tmr_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->tmr_irq_rid, RF_ACTIVE);
	if (err) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->tmr_mem_rid,
		    sc->tmr_mem_res);
		device_printf(dev, "Error: could not allocate irq resources\n");
		return (ENXIO);
	}

#ifdef PPS_SYNC
	am335x_dmtimer_pps_init(dev, sc);
#endif

	enable = 0;
	/* Try to use as a timecounter or event timer */
	if (am335x_dmtimer_system_compatible(dev)) {
		if (am335x_dmtimer_init_tc(sc) == 0)
			enable = 1;
		else if (am335x_dmtimer_init_et(sc) == 0)
			enable = 1;
	}

	if (enable) {
		/* Enable clocks and power on the chosen devices. */
		timer_id = ti_hwmods_get_clock(dev);
		if (timer_id == INVALID_CLK_IDENT) {
			bus_release_resource(dev, SYS_RES_MEMORY, sc->tmr_mem_rid,
			    sc->tmr_mem_res);
			bus_release_resource(dev, SYS_RES_IRQ, sc->tmr_irq_rid,
			    sc->tmr_irq_res);
			device_printf(dev, "failed to get device id using ti,hwmods\n");
			return (ENXIO);
		}

		err  = ti_prcm_clk_set_source(timer_id, SYSCLK_CLK);
		err |= ti_prcm_clk_enable(timer_id);

		if (err) {
			bus_release_resource(dev, SYS_RES_MEMORY, sc->tmr_mem_rid,
			    sc->tmr_mem_res);
			bus_release_resource(dev, SYS_RES_IRQ, sc->tmr_irq_rid,
			    sc->tmr_irq_res);
			device_printf(dev, "Error: could not enable timer clock\n");
			return (ENXIO);
		}
	}

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
DELAY(int usec)
{
	struct am335x_dmtimer_softc *sc;
	int32_t counts;
	uint32_t first, last;

	sc = am335x_dmtimer_tc_sc;

	if (sc == NULL) {
		for (; usec > 0; usec--)
			for (counts = 200; counts > 0; counts--)
				/* Prevent gcc from optimizing  out the loop */
				cpufunc_nullop();
		return;
	}

	/* Get the number of times to count */
	counts = (usec + 1) * (sc->sysclk_freq / 1000000);

	first = DMTIMER_READ4(sc, DMT_TCRR);

	while (counts > 0) {
		last = DMTIMER_READ4(sc, DMT_TCRR);
		if (last > first) {
			counts -= (int32_t)(last - first);
		} else {
			counts -= (int32_t)((0xFFFFFFFF - first) + last);
		}
		first = last;
	}
}

