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

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_scm.h>

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

/*
 * Use timer 2 for the eventtimer.  When PPS support is not compiled in, there's
 * no need to use a timer that has an associated capture-input pin, so use timer
 * 3 for timecounter.  When PPS is compiled in we ignore the default and use
 * whichever of timers 4-7 have the capture pin configured.
 */
#define	DEFAULT_ET_TIMER	2
#define	DEFAULT_TC_TIMER	3

struct am335x_dmtimer_softc {
	struct resource *	tmr_mem_res[AM335X_NUM_TIMERS];
	struct resource *	tmr_irq_res[AM335X_NUM_TIMERS];
	uint32_t		sysclk_freq;
	uint32_t		tc_num;		/* Which timer number is tc. */
	uint32_t		tc_tclr;	/* Cached tc TCLR register. */
	struct resource *	tc_memres;	/* Resources for tc timer. */
	uint32_t		et_num;		/* Which timer number is et. */
	uint32_t		et_tclr;	/* Cached et TCLR register. */
	struct resource *	et_memres;	/* Resources for et timer. */
	int			pps_curmode;	/* Edge mode now set in hw. */
	struct task 		pps_task;	/* For pps_event handling. */
	struct cdev *		pps_cdev;
	struct pps_state 	pps;
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
	sc->tc_tclr &= ~DMT_TCLR_CAPTRAN_MASK;
	switch (newmode) {
	case PPS_CAPTUREASSERT:
		sc->tc_tclr |= DMT_TCLR_CAPTRAN_LOHI;
		break;
	case PPS_CAPTURECLEAR:
		sc->tc_tclr |= DMT_TCLR_CAPTRAN_HILO;
		break;
	default:
		/* It can't be BOTH, so it's disabled. */
		break;
	}
	am335x_dmtimer_tc_write_4(sc, DMT_TCLR, sc->tc_tclr);
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
	if (am335x_dmtimer_tc_read_4(sc, DMT_IRQSTATUS_RAW) & DMT_IRQ_TCAR) {
		pps_capture(&sc->pps);
		sc->pps.capcount = am335x_dmtimer_tc_read_4(sc, DMT_TCAR1);
		am335x_dmtimer_tc_write_4(sc, DMT_IRQSTATUS, DMT_IRQ_TCAR);
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

/*
 * Set up the PPS cdev and the the kernel timepps stuff.
 *
 * Note that this routine cannot touch the hardware, because bus space resources
 * are not fully set up yet when this is called.
 */
static int
am335x_dmtimer_pps_init(device_t dev, struct am335x_dmtimer_softc *sc)
{
	int i, timer_num, unit;
	unsigned int padstate;
	const char * padmux;
	struct padinfo {
		char * ballname;
		char * muxname;
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
	timer_num = 0;
	for (i = 0; i < nitems(padinfo) && timer_num == 0; ++i) {
		if (ti_scm_padconf_get(padinfo[i].ballname, &padmux, 
		    &padstate) == 0) {
			if (strcasecmp(padinfo[i].muxname, padmux) == 0 &&
			    (padstate & (0x01 << 5)))
				timer_num = padinfo[i].timer_num;
		}
	}

	if (timer_num == 0) {
		device_printf(dev, "No DMTimer found with capture pin "
		    "configured as input; PPS driver disabled.\n");
		return (DEFAULT_TC_TIMER);
	}

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
	sc->tc.tc_poll_pps = am335x_dmtimer_tc_poll_pps;
	TASK_INIT(&sc->pps_task, 0, am335x_dmtimer_process_pps_event, sc);

	/* Create the PPS cdev.  */
	unit = device_get_unit(dev);
	sc->pps_cdev = make_dev(&am335x_dmtimer_pps_cdevsw, unit, 
	    UID_ROOT, GID_WHEEL, 0600, PPS_CDEV_NAME);
	sc->pps_cdev->si_drv1 = sc;

	device_printf(dev, "Using DMTimer%d for PPS device /dev/%s%d\n", 
	    timer_num, PPS_CDEV_NAME, unit);

	return (timer_num);
}

/*
 * End of PPS driver code.
 */

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
	sc->et_tclr &= ~(DMT_TCLR_START | DMT_TCLR_AUTOLOAD);
	am335x_dmtimer_et_write_4(sc, DMT_TCLR, sc->et_tclr);
	am335x_dmtimer_et_write_4(sc, DMT_IRQSTATUS, DMT_IRQ_OVF);

	if (period != 0) {
		reload_count = ((uint32_t)et->et_frequency * period) >> 32;
		sc->et_tclr |= DMT_TCLR_AUTOLOAD;
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
	am335x_dmtimer_et_write_4(sc, DMT_TLDR, 0xFFFFFFFF - reload_count);
	am335x_dmtimer_et_write_4(sc, DMT_TCRR, 0xFFFFFFFF - initial_count);

	/* Enable overflow interrupt, and start the timer. */
	am335x_dmtimer_et_write_4(sc, DMT_IRQENABLE_SET, DMT_IRQ_OVF);
	sc->et_tclr |= DMT_TCLR_START;
	am335x_dmtimer_et_write_4(sc, DMT_TCLR, sc->et_tclr);

	return (0);
}

static int
am335x_dmtimer_stop(struct eventtimer *et)
{
	struct am335x_dmtimer_softc *sc;

	sc = et->et_priv;

	/* Stop timer, disable and clear interrupt. */
	sc->et_tclr &= ~(DMT_TCLR_START | DMT_TCLR_AUTOLOAD);
	am335x_dmtimer_et_write_4(sc, DMT_TCLR, sc->et_tclr);
	am335x_dmtimer_et_write_4(sc, DMT_IRQENABLE_CLR, DMT_IRQ_OVF);
	am335x_dmtimer_et_write_4(sc, DMT_IRQSTATUS, DMT_IRQ_OVF);
	return (0);
}

static int
am335x_dmtimer_intr(void *arg)
{
	struct am335x_dmtimer_softc *sc;

	sc = arg;

	/* Ack the interrupt, and invoke the callback if it's still enabled. */
	am335x_dmtimer_et_write_4(sc, DMT_IRQSTATUS, DMT_IRQ_OVF);
	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}

static int
am335x_dmtimer_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

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

	/*
	 * Use the default eventtimer.  Let the PPS init routine decide which
	 * timer to use for the timecounter.
	 */
	sc->et_num = DEFAULT_ET_TIMER;
	sc->tc_num = am335x_dmtimer_pps_init(dev, sc);

	sc->et_memres = sc->tmr_mem_res[sc->et_num];
	sc->tc_memres = sc->tmr_mem_res[sc->tc_num];

	/* Enable clocks and power on the chosen devices. */
	err  = ti_prcm_clk_set_source(DMTIMER0_CLK + sc->et_num, SYSCLK_CLK);
	err |= ti_prcm_clk_enable(DMTIMER0_CLK + sc->et_num);
	err |= ti_prcm_clk_set_source(DMTIMER0_CLK + sc->tc_num, SYSCLK_CLK);
	err |= ti_prcm_clk_enable(DMTIMER0_CLK + sc->tc_num);
	if (err) {
		device_printf(dev, "Error: could not enable timer clock\n");
		return (ENXIO);
	}

	/* Setup eventtimer interrupt handler. */
	if (bus_setup_intr(dev, sc->tmr_irq_res[sc->et_num], INTR_TYPE_CLK,
			am335x_dmtimer_intr, NULL, sc, &ihl) != 0) {
		device_printf(dev, "Unable to setup the clock irq handler.\n");
		return (ENXIO);
	}

	/* Set up timecounter, start it, register it. */
	am335x_dmtimer_tc_write_4(sc, DMT_TSICR, DMT_TSICR_RESET);
	while (am335x_dmtimer_tc_read_4(sc, DMT_TIOCP_CFG) & DMT_TIOCP_RESET)
		continue;

	sc->tc_tclr |= DMT_TCLR_START | DMT_TCLR_AUTOLOAD;
	am335x_dmtimer_tc_write_4(sc, DMT_TLDR, 0);
	am335x_dmtimer_tc_write_4(sc, DMT_TCRR, 0);
	am335x_dmtimer_tc_write_4(sc, DMT_TCLR, sc->tc_tclr);

	sc->tc.tc_name           = "AM335x Timecounter";
	sc->tc.tc_get_timecount  = am335x_dmtimer_tc_get_timecount;
	sc->tc.tc_counter_mask   = ~0u;
	sc->tc.tc_frequency      = sc->sysclk_freq;
	sc->tc.tc_quality        = 1000;
	sc->tc.tc_priv           = sc;
	tc_init(&sc->tc);

	sc->et.et_name = "AM335x Eventtimer";
	sc->et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT;
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

