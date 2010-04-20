/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)clock.c	7.2 (Berkeley) 5/12/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Routines to handle clock hardware.
 */

#ifndef __amd64__
#include "opt_apic.h"
#endif
#include "opt_clock.h"
#include "opt_kdtrace.h"
#include "opt_isa.h"
#include "opt_mca.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kdb.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/apicvar.h>
#include <machine/ppireg.h>
#include <machine/timerreg.h>
#include <machine/smp.h>

#include <isa/rtc.h>
#ifdef DEV_ISA
#include <isa/isareg.h>
#include <isa/isavar.h>
#endif

#ifdef DEV_MCA
#include <i386/bios/mca_machdep.h>
#endif

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
#endif

#define	TIMER_DIV(x) ((i8254_freq + (x) / 2) / (x))

int	clkintr_pending;
static int pscnt = 1;
static int psdiv = 1;
#ifndef TIMER_FREQ
#define TIMER_FREQ   1193182
#endif
u_int	i8254_freq = TIMER_FREQ;
TUNABLE_INT("hw.i8254.freq", &i8254_freq);
int	i8254_max_count;
static int i8254_real_max_count;

static int lapic_allclocks = 1;
TUNABLE_INT("machdep.lapic_allclocks", &lapic_allclocks);

struct mtx clock_lock;
static	struct intsrc *i8254_intsrc;
static	u_int32_t i8254_lastcount;
static	u_int32_t i8254_offset;
static	int	(*i8254_pending)(struct intsrc *);
static	int	i8254_ticked;
static	int	using_atrtc_timer;
static	enum lapic_clock using_lapic_timer = LAPIC_CLOCK_NONE;

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

static	u_char	timer2_state;

static	unsigned i8254_get_timecount(struct timecounter *tc);
static	unsigned i8254_simple_get_timecount(struct timecounter *tc);
static	void	set_i8254_freq(u_int freq, int intr_freq);

static struct timecounter i8254_timecounter = {
	i8254_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"i8254",		/* name */
	0			/* quality */
};

int
hardclockintr(struct trapframe *frame)
{

	if (PCPU_GET(cpuid) == 0)
		hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	else
		hardclock_cpu(TRAPF_USERMODE(frame));
	return (FILTER_HANDLED);
}

int
statclockintr(struct trapframe *frame)
{

	profclockintr(frame);
	statclock(TRAPF_USERMODE(frame));
	return (FILTER_HANDLED);
}

int
profclockintr(struct trapframe *frame)
{

	if (!using_atrtc_timer)
		hardclockintr(frame);
	if (profprocs != 0)
		profclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	return (FILTER_HANDLED);
}

static int
clkintr(struct trapframe *frame)
{

	if (timecounter->tc_get_timecount == i8254_get_timecount) {
		mtx_lock_spin(&clock_lock);
		if (i8254_ticked)
			i8254_ticked = 0;
		else {
			i8254_offset += i8254_max_count;
			i8254_lastcount = 0;
		}
		clkintr_pending = 0;
		mtx_unlock_spin(&clock_lock);
	}
	KASSERT(using_lapic_timer == LAPIC_CLOCK_NONE,
	    ("clk interrupt enabled with lapic timer"));

#ifdef KDTRACE_HOOKS
	/*
	 * If the DTrace hooks are configured and a callback function
	 * has been registered, then call it to process the high speed
	 * timers.
	 */
	int cpu = PCPU_GET(cpuid);
	if (cyclic_clock_func[cpu] != NULL)
		(*cyclic_clock_func[cpu])(frame);
#endif

	if (using_atrtc_timer) {
#ifdef SMP
		if (smp_started)
			ipi_all_but_self(IPI_HARDCLOCK);
#endif
		hardclockintr(frame);
	} else {
		if (--pscnt <= 0) {
			pscnt = psratio;
#ifdef SMP
			if (smp_started)
				ipi_all_but_self(IPI_STATCLOCK);
#endif
			statclockintr(frame);
		} else {
#ifdef SMP
			if (smp_started)
				ipi_all_but_self(IPI_PROFCLOCK);
#endif
			profclockintr(frame);
		}
	}

#ifdef DEV_MCA
	/* Reset clock interrupt by asserting bit 7 of port 0x61 */
	if (MCA_system)
		outb(0x61, inb(0x61) | 0x80);
#endif
	return (FILTER_HANDLED);
}

int
timer_spkr_acquire(void)
{
	int mode;

	mode = TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT;

	if (timer2_state != RELEASED)
		return (-1);
	timer2_state = ACQUIRED;

	/*
	 * This access to the timer registers is as atomic as possible
	 * because it is a single instruction.  We could do better if we
	 * knew the rate.  Use of splclock() limits glitches to 10-100us,
	 * and this is probably good enough for timer2, so we aren't as
	 * careful with it as with timer0.
	 */
	outb(TIMER_MODE, TIMER_SEL2 | (mode & 0x3f));
	ppi_spkr_on();		/* enable counter2 output to speaker */
	return (0);
}

int
timer_spkr_release(void)
{

	if (timer2_state != ACQUIRED)
		return (-1);
	timer2_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT);
	ppi_spkr_off();		/* disable counter2 output to speaker */
	return (0);
}

void
timer_spkr_setfreq(int freq)
{

	freq = i8254_freq / freq;
	mtx_lock_spin(&clock_lock);
	outb(TIMER_CNTR2, freq & 0xff);
	outb(TIMER_CNTR2, freq >> 8);
	mtx_unlock_spin(&clock_lock);
}

/*
 * This routine receives statistical clock interrupts from the RTC.
 * As explained above, these occur at 128 interrupts per second.
 * When profiling, we receive interrupts at a rate of 1024 Hz.
 *
 * This does not actually add as much overhead as it sounds, because
 * when the statistical clock is active, the hardclock driver no longer
 * needs to keep (inaccurate) statistics on its own.  This decouples
 * statistics gathering from scheduling interrupts.
 *
 * The RTC chip requires that we read status register C (RTC_INTR)
 * to acknowledge an interrupt, before it will generate the next one.
 * Under high interrupt load, rtcintr() can be indefinitely delayed and
 * the clock can tick immediately after the read from RTC_INTR.  In this
 * case, the mc146818A interrupt signal will not drop for long enough
 * to register with the 8259 PIC.  If an interrupt is missed, the stat
 * clock will halt, considerably degrading system performance.  This is
 * why we use 'while' rather than a more straightforward 'if' below.
 * Stat clock ticks can still be lost, causing minor loss of accuracy
 * in the statistics, but the stat clock will no longer stop.
 */
static int
rtcintr(struct trapframe *frame)
{
	int flag = 0;

	while (rtcin(RTC_INTR) & RTCIR_PERIOD) {
		flag = 1;
		if (--pscnt <= 0) {
			pscnt = psdiv;
#ifdef SMP
			if (smp_started)
				ipi_all_but_self(IPI_STATCLOCK);
#endif
			statclockintr(frame);
		} else {
#ifdef SMP
			if (smp_started)
				ipi_all_but_self(IPI_PROFCLOCK);
#endif
			profclockintr(frame);
		}
	}
	return(flag ? FILTER_HANDLED : FILTER_STRAY);
}

static int
getit(void)
{
	int high, low;

	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);

	mtx_unlock_spin(&clock_lock);
	return ((high << 8) | low);
}

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (i8254_freq / hz)
 * Note: timer had better have been programmed before this is first used!
 */
void
DELAY(int n)
{
	int delta, prev_tick, tick, ticks_left;

#ifdef DELAYDEBUG
	int getit_calls = 1;
	int n1;
	static int state = 0;
#endif

	if (tsc_freq != 0 && !tsc_is_broken) {
		uint64_t start, end, now;

		sched_pin();
		start = rdtsc();
		end = start + (tsc_freq * n) / 1000000;
		do {
			cpu_spinwait();
			now = rdtsc();
		} while (now < end || (now > start && end < start));
		sched_unpin();
		return;
	}
#ifdef DELAYDEBUG
	if (state == 0) {
		state = 1;
		for (n1 = 1; n1 <= 10000000; n1 *= 10)
			DELAY(n1);
		state = 2;
	}
	if (state == 1)
		printf("DELAY(%d)...", n);
#endif
	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.  Guess the initial overhead is 20 usec (on most systems it
	 * takes about 1.5 usec for each of the i/o's in getit().  The loop
	 * takes about 6 usec on a 486/33 and 13 usec on a 386/20.  The
	 * multiplications and divisions to scale the count take a while).
	 *
	 * However, if ddb is active then use a fake counter since reading
	 * the i8254 counter involves acquiring a lock.  ddb must not do
	 * locking for many reasons, but it calls here for at least atkbd
	 * input.
	 */
#ifdef KDB
	if (kdb_active)
		prev_tick = 1;
	else
#endif
		prev_tick = getit();
	n -= 0;			/* XXX actually guess no initial overhead */
	/*
	 * Calculate (n * (i8254_freq / 1e6)) without using floating point
	 * and without any avoidable overflows.
	 */
	if (n <= 0)
		ticks_left = 0;
	else if (n < 256)
		/*
		 * Use fixed point to avoid a slow division by 1000000.
		 * 39099 = 1193182 * 2^15 / 10^6 rounded to nearest.
		 * 2^15 is the first power of 2 that gives exact results
		 * for n between 0 and 256.
		 */
		ticks_left = ((u_int)n * 39099 + (1 << 15) - 1) >> 15;
	else
		/*
		 * Don't bother using fixed point, although gcc-2.7.2
		 * generates particularly poor code for the long long
		 * division, since even the slow way will complete long
		 * before the delay is up (unless we're interrupted).
		 */
		ticks_left = ((u_int)n * (long long)i8254_freq + 999999)
			     / 1000000;

	while (ticks_left > 0) {
#ifdef KDB
		if (kdb_active) {
			inb(0x84);
			tick = prev_tick - 1;
			if (tick <= 0)
				tick = i8254_max_count;
		} else
#endif
			tick = getit();
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		delta = prev_tick - tick;
		prev_tick = tick;
		if (delta < 0) {
			delta += i8254_max_count;
			/*
			 * Guard against i8254_max_count being wrong.
			 * This shouldn't happen in normal operation,
			 * but it may happen if set_i8254_freq() is
			 * traced.
			 */
			if (delta < 0)
				delta = 0;
		}
		ticks_left -= delta;
	}
#ifdef DELAYDEBUG
	if (state == 1)
		printf(" %d calls to getit() at %d usec each\n",
		       getit_calls, (n + 5) / getit_calls);
#endif
}

static void
set_i8254_freq(u_int freq, int intr_freq)
{
	int new_i8254_real_max_count;

	i8254_timecounter.tc_frequency = freq;
	mtx_lock_spin(&clock_lock);
	i8254_freq = freq;
	if (using_lapic_timer != LAPIC_CLOCK_NONE)
		new_i8254_real_max_count = 0x10000;
	else
		new_i8254_real_max_count = TIMER_DIV(intr_freq);
	if (new_i8254_real_max_count != i8254_real_max_count) {
		i8254_real_max_count = new_i8254_real_max_count;
		if (i8254_real_max_count == 0x10000)
			i8254_max_count = 0xffff;
		else
			i8254_max_count = i8254_real_max_count;
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
		outb(TIMER_CNTR0, i8254_real_max_count & 0xff);
		outb(TIMER_CNTR0, i8254_real_max_count >> 8);
	}
	mtx_unlock_spin(&clock_lock);
}

static void
i8254_restore(void)
{

	mtx_lock_spin(&clock_lock);
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR0, i8254_real_max_count & 0xff);
	outb(TIMER_CNTR0, i8254_real_max_count >> 8);
	mtx_unlock_spin(&clock_lock);
}

#ifndef __amd64__
/*
 * Restore all the timers non-atomically (XXX: should be atomically).
 *
 * This function is called from pmtimer_resume() to restore all the timers.
 * This should not be necessary, but there are broken laptops that do not
 * restore all the timers on resume.
 * As long as pmtimer is not part of amd64 suport, skip this for the amd64
 * case.
 */
void
timer_restore(void)
{

	i8254_restore();		/* restore i8254_freq and hz */
	atrtc_restore();		/* reenable RTC interrupts */
}
#endif

/* This is separate from startrtclock() so that it can be called early. */
void
i8254_init(void)
{

	mtx_init(&clock_lock, "clk", NULL, MTX_SPIN | MTX_NOPROFILE);
	set_i8254_freq(i8254_freq, hz);
}

void
startrtclock()
{

	atrtc_start();

	set_i8254_freq(i8254_freq, hz);
	tc_init(&i8254_timecounter);

	init_TSC();
}

/*
 * Start both clocks running.
 */
void
cpu_initclocks()
{
#if defined(__amd64__) || defined(DEV_APIC)
	enum lapic_clock tlsca;
#endif
	int tasc;

	/* Initialize RTC. */
	atrtc_start();
	tasc = atrtc_setup_clock();

	/*
	 * If the atrtc successfully initialized and the users didn't force
	 * otherwise use the LAPIC in order to cater hardclock only, otherwise
	 * take in charge all the clock sources.
	 */
#if defined(__amd64__) || defined(DEV_APIC)
	tlsca = (lapic_allclocks == 0 && tasc != 0) ? LAPIC_CLOCK_HARDCLOCK :
	    LAPIC_CLOCK_ALL;
	using_lapic_timer = lapic_setup_clock(tlsca);
#endif
	/*
	 * If we aren't using the local APIC timer to drive the kernel
	 * clocks, setup the interrupt handler for the 8254 timer 0 so
	 * that it can drive hardclock().  Otherwise, change the 8254
	 * timecounter to user a simpler algorithm.
	 */
	if (using_lapic_timer == LAPIC_CLOCK_NONE) {
		intr_add_handler("clk", 0, (driver_filter_t *)clkintr, NULL,
		    NULL, INTR_TYPE_CLK, NULL);
		i8254_intsrc = intr_lookup_source(0);
		if (i8254_intsrc != NULL)
			i8254_pending =
			    i8254_intsrc->is_pic->pic_source_pending;
	} else {
		i8254_timecounter.tc_get_timecount =
		    i8254_simple_get_timecount;
		i8254_timecounter.tc_counter_mask = 0xffff;
		set_i8254_freq(i8254_freq, hz);
	}

	/*
	 * If the separate statistics clock hasn't been explicility disabled
	 * and we aren't already using the local APIC timer to drive the
	 * kernel clocks, then setup the RTC to periodically interrupt to
	 * drive statclock() and profclock().
	 */
	if (using_lapic_timer != LAPIC_CLOCK_ALL) {
		using_atrtc_timer = tasc; 
		if (using_atrtc_timer) {
			/* Enable periodic interrupts from the RTC. */
			intr_add_handler("rtc", 8,
			    (driver_filter_t *)rtcintr, NULL, NULL,
			    INTR_TYPE_CLK, NULL);
			atrtc_enable_intr();
		} else {
			profhz = hz;
			if (hz < 128)
				stathz = hz;
			else
				stathz = hz / (hz / 128);
		}
	}

	init_TSC_tc();
}

void
cpu_startprofclock(void)
{

	if (using_lapic_timer == LAPIC_CLOCK_ALL || !using_atrtc_timer)
		return;
	atrtc_rate(RTCSA_PROF);
	psdiv = pscnt = psratio;
}

void
cpu_stopprofclock(void)
{

	if (using_lapic_timer == LAPIC_CLOCK_ALL || !using_atrtc_timer)
		return;
	atrtc_rate(RTCSA_NOPROF);
	psdiv = pscnt = 1;
}

static int
sysctl_machdep_i8254_freq(SYSCTL_HANDLER_ARGS)
{
	int error;
	u_int freq;

	/*
	 * Use `i8254' instead of `timer' in external names because `timer'
	 * is is too generic.  Should use it everywhere.
	 */
	freq = i8254_freq;
	error = sysctl_handle_int(oidp, &freq, 0, req);
	if (error == 0 && req->newptr != NULL)
		set_i8254_freq(freq, hz);
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, i8254_freq, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_i8254_freq, "IU", "");

static unsigned
i8254_simple_get_timecount(struct timecounter *tc)
{

	return (i8254_max_count - getit());
}

static unsigned
i8254_get_timecount(struct timecounter *tc)
{
	register_t flags;
	u_int count;
	u_int high, low;

#ifdef __amd64__
	flags = read_rflags();
#else
	flags = read_eflags();
#endif
	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);
	count = i8254_max_count - ((high << 8) | low);
	if (count < i8254_lastcount ||
	    (!i8254_ticked && (clkintr_pending ||
	    ((count < 20 || (!(flags & PSL_I) &&
	    count < i8254_max_count / 2u)) &&
	    i8254_pending != NULL && i8254_pending(i8254_intsrc))))) {
		i8254_ticked = 1;
		i8254_offset += i8254_max_count;
	}
	i8254_lastcount = count;
	count += i8254_offset;
	mtx_unlock_spin(&clock_lock);
	return (count);
}

#ifdef DEV_ISA
/*
 * Attach to the ISA PnP descriptors for the timer
 */
static struct isa_pnp_id attimer_ids[] = {
	{ 0x0001d041 /* PNP0100 */, "AT timer" },
	{ 0 }
};

static int
attimer_probe(device_t dev)
{
	int result;
	
	result = ISA_PNP_PROBE(device_get_parent(dev), dev, attimer_ids);
	if (result <= 0)
		device_quiet(dev);
	return(result);
}

static int
attimer_attach(device_t dev)
{
	return(0);
}

static int
attimer_resume(device_t dev)
{

	i8254_restore();
	return (0);
}

static device_method_t attimer_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		attimer_probe),
	DEVMETHOD(device_attach,	attimer_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	attimer_resume),
	{ 0, 0 }
};

static driver_t attimer_driver = {
	"attimer",
	attimer_methods,
	1,		/* no softc */
};

static devclass_t attimer_devclass;

DRIVER_MODULE(attimer, isa, attimer_driver, attimer_devclass, 0, 0);
DRIVER_MODULE(attimer, acpi, attimer_driver, attimer_devclass, 0, 0);

#endif /* DEV_ISA */
