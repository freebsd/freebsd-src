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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	$Id: clock.c,v 1.28 1997/07/20 11:55:52 kato Exp $
 */

/*
 * Routines to handle clock hardware.
 */

/*
 * inittodr, settodr and support routines written
 * by Christoph Robitschko <chmr@edvz.tu-graz.ac.at>
 *
 * reintroduced and updated by Chris Stenton <chris@gnome.co.uk> 8/10/94
 */

/*
 * modified for PC98 by Kakefuda
 */

#include "opt_clock.h"
#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/clock.h>
#ifdef CLK_CALIBRATION_LOOP
#include <machine/cons.h>
#endif
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/ipl.h>
#ifdef APIC_IO
#include <machine/smp.h>
#include <machine/smptests.h>		/** TEST_ALTTIMER, APIC_PIN0_TIMER */
#endif /* APIC_IO */

#include <i386/isa/icu.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <i386/isa/isa_device.h>
#else
#include <i386/isa/isa.h>
#include <i386/isa/rtc.h>
#endif
#include <i386/isa/timerreg.h>

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y) ((u_int)(y) % 4 == 0)
#define DAYSPERYEAR   (31+28+31+30+31+30+31+31+30+31+30+31)

#define	TIMER_DIV(x) ((timer_freq + (x) / 2) / (x))

/*
 * Time in timer cycles that it takes for microtime() to disable interrupts
 * and latch the count.  microtime() currently uses "cli; outb ..." so it
 * normally takes less than 2 timer cycles.  Add a few for cache misses.
 * Add a few more to allow for latency in bogus calls to microtime() with
 * interrupts already disabled.
 */
#define	TIMER0_LATCH_COUNT	20

/*
 * Maximum frequency that we are willing to allow for timer0.  Must be
 * low enough to guarantee that the timer interrupt handler returns
 * before the next timer interrupt.  Must result in a lower TIMER_DIV
 * value than TIMER0_LATCH_COUNT so that we don't have to worry about
 * underflow in the calculation of timer0_overflow_threshold.
 */
#define	TIMER0_MAX_FREQ		20000

int	adjkerntz;		/* local offset	from GMT in seconds */
int	disable_rtc_set;	/* disable resettodr() if != 0 */
u_int	idelayed;
#if defined(I586_CPU) || defined(I686_CPU)
#ifndef SMP
u_int	i586_ctr_bias;
u_int	i586_ctr_comultiplier;
#endif
u_int	i586_ctr_freq;
#ifndef SMP
u_int	i586_ctr_multiplier;
#endif
#endif
int	statclock_disable;
u_int	stat_imask = SWI_CLOCK_MASK;
#ifdef TIMER_FREQ
u_int	timer_freq = TIMER_FREQ;
#else
#ifdef PC98
#ifndef AUTO_CLOCK
#ifndef PC98_8M
u_int	timer_freq = 2457600;
#else	/* !PC98_8M */
u_int	timer_freq = 1996800;
#endif	/* PC98_8M */
#else	/* AUTO_CLOCK */
u_int	timer_freq = 2457600;
#endif	/* AUTO_CLOCK */
#else /* IBM-PC */
u_int	timer_freq = 1193182;
#endif /* PC98 */
#endif
int	timer0_max_count;
u_int	timer0_overflow_threshold;
u_int	timer0_prescaler_count;
int	wall_cmos_clock;	/* wall	CMOS clock assumed if != 0 */

static	int	beeping = 0;
static	u_int	clk_imask = HWI_MASK | SWI_MASK;
static	const u_char daysinmonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
static	u_int	hardclock_max_count;
/*
 * XXX new_function and timer_func should not handle clockframes, but
 * timer_func currently needs to hold hardclock to handle the
 * timer0_state == 0 case.  We should use register_intr()/unregister_intr()
 * to switch between clkintr() and a slightly different timerintr().
 */
static	void	(*new_function) __P((struct clockframe *frame));
static	u_int	new_rate;
#ifndef PC98
static	u_char	rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;
static	u_char	rtc_statusb = RTCSB_24HR | RTCSB_PINTR;
#endif

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

static	u_char	timer0_state;
#ifdef	PC98
static 	u_char	timer1_state;
#endif
static	u_char	timer2_state;
static	void	(*timer_func) __P((struct clockframe *frame)) = hardclock;
#ifdef PC98
static void rtc_serialcombit __P((int));
static void rtc_serialcom __P((int));
static int rtc_inb __P((void));
static void rtc_outb __P((int));
#endif

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
static	void	set_i586_ctr_freq(u_int i586_freq, u_int i8254_freq);
#endif
static	void	set_timer_freq(u_int freq, int intr_freq);

static void
clkintr(struct clockframe frame)
{
	timer_func(&frame);
	switch (timer0_state) {

	case RELEASED:
		setdelayed();
		break;

	case ACQUIRED:
		if ((timer0_prescaler_count += timer0_max_count)
		    >= hardclock_max_count) {
			hardclock(&frame);
			setdelayed();
			timer0_prescaler_count -= hardclock_max_count;
		}
		break;

	case ACQUIRE_PENDING:
		setdelayed();
		timer0_max_count = TIMER_DIV(new_rate);
		timer0_overflow_threshold =
			timer0_max_count - TIMER0_LATCH_COUNT;
		disable_intr();
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
		outb(TIMER_CNTR0, timer0_max_count & 0xff);
		outb(TIMER_CNTR0, timer0_max_count >> 8);
		enable_intr();
		timer0_prescaler_count = 0;
		timer_func = new_function;
		timer0_state = ACQUIRED;
		break;

	case RELEASE_PENDING:
		if ((timer0_prescaler_count += timer0_max_count)
		    >= hardclock_max_count) {
			hardclock(&frame);
			setdelayed();
			timer0_max_count = hardclock_max_count;
			timer0_overflow_threshold =
				timer0_max_count - TIMER0_LATCH_COUNT;
			disable_intr();
			outb(TIMER_MODE,
			     TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
			outb(TIMER_CNTR0, timer0_max_count & 0xff);
			outb(TIMER_CNTR0, timer0_max_count >> 8);
			enable_intr();
			/*
			 * See microtime.s for this magic.
			 */
#ifdef PC98
#ifndef AUTO_CLOCK
#ifndef PC98_8M
			time.tv_usec += (6667 *
				(timer0_prescaler_count - hardclock_max_count))
				>> 14;
#else /* PC98_8M */
			time.tv_usec += (16411 *
				(timer0_prescaler_count - hardclock_max_count))
				>> 15;
#endif /* PC98_8M */
#else /* AUTO_CLOCK */
			if (pc98_machine_type & M_8M) {
				/* PC98_8M */
				time.tv_usec += (16411 *
					(timer0_prescaler_count -
					 hardclock_max_count)) >> 15;
			} else {
				time.tv_usec += (6667 *
					(timer0_prescaler_count -
					 hardclock_max_count)) >> 14;
			}
#endif /* AUTO_CLOCK */
#else /* IBM-PC */
			time.tv_usec += (27465 *
				(timer0_prescaler_count - hardclock_max_count))
				>> 15;
#endif /* PC98 */
			if (time.tv_usec >= 1000000)
				time.tv_usec -= 1000000;
			timer0_prescaler_count = 0;
			timer_func = hardclock;
			timer0_state = RELEASED;
		}
		break;
	}
}

/*
 * The acquire and release functions must be called at ipl >= splclock().
 */
int
acquire_timer0(int rate, void (*function) __P((struct clockframe *frame)))
{
	static int old_rate;

	if (rate <= 0 || rate > TIMER0_MAX_FREQ)
		return (-1);
	switch (timer0_state) {

	case RELEASED:
		timer0_state = ACQUIRE_PENDING;
		break;

	case RELEASE_PENDING:
		if (rate != old_rate)
			return (-1);
		/*
		 * The timer has been released recently, but is being
		 * re-acquired before the release completed.  In this
		 * case, we simply reclaim it as if it had not been
		 * released at all.
		 */
		timer0_state = ACQUIRED;
		break;

	default:
		return (-1);	/* busy */
	}
	new_function = function;
	old_rate = new_rate = rate;
	return (0);
}

#ifdef PC98
int
acquire_timer1(int mode)
{

	if (timer1_state != RELEASED)
		return (-1);
	timer1_state = ACQUIRED;

	/*
	 * This access to the timer registers is as atomic as possible
	 * because it is a single instruction.  We could do better if we
	 * knew the rate.  Use of splclock() limits glitches to 10-100us,
	 * and this is probably good enough for timer2, so we aren't as
	 * careful with it as with timer0.
	 */
	outb(TIMER_MODE, TIMER_SEL1 | (mode & 0x3f));

	return (0);
}
#endif

int
acquire_timer2(int mode)
{

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

	return (0);
}

int
release_timer0()
{
	switch (timer0_state) {

	case ACQUIRED:
		timer0_state = RELEASE_PENDING;
		break;

	case ACQUIRE_PENDING:
		/* Nothing happened yet, release quickly. */
		timer0_state = RELEASED;
		break;

	default:
		return (-1);
	}
	return (0);
}

#ifdef PC98
int
release_timer1()
{

	if (timer1_state != ACQUIRED)
		return (-1);
	timer1_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL1 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
}
#endif

int
release_timer2()
{

	if (timer2_state != ACQUIRED)
		return (-1);
	timer2_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
}

#ifndef PC98
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
static void
rtcintr(struct clockframe frame)
{
	while (rtcin(RTC_INTR) & RTCIR_PERIOD)
		statclock(&frame);
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(rtc, rtc)
{
	printf("%02x/%02x/%02x %02x:%02x:%02x, A = %02x, B = %02x, C = %02x\n",
	       rtcin(RTC_YEAR), rtcin(RTC_MONTH), rtcin(RTC_DAY),
	       rtcin(RTC_HRS), rtcin(RTC_MIN), rtcin(RTC_SEC),
	       rtcin(RTC_STATUSA), rtcin(RTC_STATUSB), rtcin(RTC_INTR));
}
#endif /* DDB */
#endif /* for PC98 */

static int
getit(void)
{
	u_long ef;
	int high, low;

	ef = read_eflags();
	disable_intr();

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);

	write_eflags(ef);
	return ((high << 8) | low);
}

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (timer_freq / hz)
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
	 * Guard against the timer being uninitialized if we are called
	 * early for console i/o.
	 */
	if (timer0_max_count == 0)
		set_timer_freq(timer_freq, hz);

	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.  Guess the initial overhead is 20 usec (on most systems it
	 * takes about 1.5 usec for each of the i/o's in getit().  The loop
	 * takes about 6 usec on a 486/33 and 13 usec on a 386/20.  The
	 * multiplications and divisions to scale the count take a while).
	 */
	prev_tick = getit();
	n -= 0;			/* XXX actually guess no initial overhead */
	/*
	 * Calculate (n * (timer_freq / 1e6)) without using floating point
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
		ticks_left = ((u_int)n * (long long)timer_freq + 999999)
			     / 1000000;

	while (ticks_left > 0) {
		tick = getit();
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		delta = prev_tick - tick;
		prev_tick = tick;
		if (delta < 0) {
			delta += timer0_max_count;
			/*
			 * Guard against timer0_max_count being wrong.
			 * This shouldn't happen in normal operation,
			 * but it may happen if set_timer_freq() is
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
sysbeepstop(void *chan)
{
#ifdef PC98	/* PC98 */
	outb(IO_PPI, inb(IO_PPI)|0x08);	/* disable counter1 output to speaker */
	release_timer1();
#else
	outb(IO_PPI, inb(IO_PPI)&0xFC);	/* disable counter2 output to speaker */
	release_timer2();
#endif
	beeping = 0;
}

int
sysbeep(int pitch, int period)
{
	int x = splclock();

#ifdef PC98
	if (acquire_timer1(TIMER_SQWAVE|TIMER_16BIT))
		if (!beeping) {
			/* Something else owns it. */
			splx(x);
			return (-1); /* XXX Should be EBUSY, but nobody cares anyway. */
		}
	disable_intr();
	outb(0x3fdb, pitch);
	outb(0x3fdb, (pitch>>8));
	enable_intr();
	if (!beeping) {
		/* enable counter1 output to speaker */
		outb(IO_PPI, (inb(IO_PPI) & 0xf7));
		beeping = period;
		timeout(sysbeepstop, (void *)NULL, period);
	}
#else
	if (acquire_timer2(TIMER_SQWAVE|TIMER_16BIT))
		if (!beeping) {
			/* Something else owns it. */
			splx(x);
			return (-1); /* XXX Should be EBUSY, but nobody cares anyway. */
		}
	disable_intr();
	outb(TIMER_CNTR2, pitch);
	outb(TIMER_CNTR2, (pitch>>8));
	enable_intr();
	if (!beeping) {
		/* enable counter2 output to speaker */
		outb(IO_PPI, inb(IO_PPI) | 3);
		beeping = period;
		timeout(sysbeepstop, (void *)NULL, period);
	}
#endif
	splx(x);
	return (0);
}

#ifndef PC98
/*
 * RTC support routines
 */

int
rtcin(reg)
	int reg;
{
	u_char val;

	outb(IO_RTC, reg);
	inb(0x84);
	val = inb(IO_RTC + 1);
	inb(0x84);
	return (val);
}

static __inline void
writertc(u_char reg, u_char val)
{
	outb(IO_RTC, reg);
	outb(IO_RTC + 1, val);
}

static __inline int
readrtc(int port)
{
	return(bcd2bin(rtcin(port)));
}
#endif

#ifdef PC98
unsigned int delaycount;
#define FIRST_GUESS	0x2000
static void findcpuspeed(void)
{
	int i;
	int remainder;

	/* Put counter in count down mode */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_16BIT | TIMER_RATEGEN);
	outb(TIMER_CNTR0, 0xff);
	outb(TIMER_CNTR0, 0xff);
	for (i = FIRST_GUESS; i; i--)
		;
	remainder = getit();
	delaycount = (FIRST_GUESS * TIMER_DIV(1000)) / (0xffff - remainder);
}
#endif

#ifndef PC98
static u_int
calibrate_clocks(void)
{
	u_int count, prev_count, tot_count;
	int sec, start_sec, timeout;

	if (bootverbose)
	        printf("Calibrating clock(s) ... ");
	if (!(rtcin(RTC_STATUSD) & RTCSD_PWR))
		goto fail;
	timeout = 100000000;

	/* Read the mc146818A seconds counter. */
	for (;;) {
		if (!(rtcin(RTC_STATUSA) & RTCSA_TUP)) {
			sec = rtcin(RTC_SEC);
			break;
		}
		if (--timeout == 0)
			goto fail;
	}

	/* Wait for the mC146818A seconds counter to change. */
	start_sec = sec;
	for (;;) {
		if (!(rtcin(RTC_STATUSA) & RTCSA_TUP)) {
			sec = rtcin(RTC_SEC);
			if (sec != start_sec)
				break;
		}
		if (--timeout == 0)
			goto fail;
	}

	/* Start keeping track of the i8254 counter. */
	prev_count = getit();
	if (prev_count == 0 || prev_count > timer0_max_count)
		goto fail;
	tot_count = 0;

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
	if (cpu_class == CPUCLASS_586 || cpu_class == CPUCLASS_686)
		wrmsr(0x10, 0LL);	/* XXX 0x10 is the MSR for the TSC */
#endif

	/*
	 * Wait for the mc146818A seconds counter to change.  Read the i8254
	 * counter for each iteration since this is convenient and only
	 * costs a few usec of inaccuracy. The timing of the final reads
	 * of the counters almost matches the timing of the initial reads,
	 * so the main cause of inaccuracy is the varying latency from 
	 * inside getit() or rtcin(RTC_STATUSA) to the beginning of the
	 * rtcin(RTC_SEC) that returns a changed seconds count.  The
	 * maximum inaccuracy from this cause is < 10 usec on 486's.
	 */
	start_sec = sec;
	for (;;) {
		if (!(rtcin(RTC_STATUSA) & RTCSA_TUP))
			sec = rtcin(RTC_SEC);
		count = getit();
		if (count == 0 || count > timer0_max_count)
			goto fail;
		if (count > prev_count)
			tot_count += prev_count - (count - timer0_max_count);
		else
			tot_count += prev_count - count;
		prev_count = count;
		if (sec != start_sec)
			break;
		if (--timeout == 0)
			goto fail;
	}

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
	/*
	 * Read the cpu cycle counter.  The timing considerations are
	 * similar to those for the i8254 clock.
	 */
	if (cpu_class == CPUCLASS_586 || cpu_class == CPUCLASS_686) {
		set_i586_ctr_freq((u_int)rdtsc(), tot_count);
		if (bootverbose)
		        printf("i586 clock: %u Hz, ", i586_ctr_freq);
	}
#endif

	if (bootverbose)
	        printf("i8254 clock: %u Hz\n", tot_count);
	return (tot_count);

fail:
	if (bootverbose)
	        printf("failed, using default i8254 clock of %u Hz\n",
		       timer_freq);
	return (timer_freq);
}
#endif	/* !PC98 */

static void
set_timer_freq(u_int freq, int intr_freq)
{
	u_long ef;

	ef = read_eflags();
	disable_intr();
	timer_freq = freq;
	timer0_max_count = hardclock_max_count = TIMER_DIV(intr_freq);
	timer0_overflow_threshold = timer0_max_count - TIMER0_LATCH_COUNT;
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR0, timer0_max_count & 0xff);
	outb(TIMER_CNTR0, timer0_max_count >> 8);
	write_eflags(ef);
}

/*
 * Initialize 8253 timer 0 early so that it can be used in DELAY().
 * XXX initialization of other timers is unintentionally left blank.
 */
void
startrtclock()
{
	u_int delta, freq;

#ifdef PC98
	findcpuspeed();
#ifndef AUTO_CLOCK
	if (pc98_machine_type & M_8M) {
#ifndef	PC98_8M
		printf("you must reconfig a kernel with \"PC98_8M\" option.\n");
#endif
	} else {
#ifdef	PC98_8M
		printf("You must reconfig a kernel without \"PC98_8M\" option.\n");
#endif
	}
#else /* AUTO_CLOCK */
	if (pc98_machine_type & M_8M)
		timer_freq = 1996800L; /* 1.9968 MHz */
	else
		timer_freq = 2457600L; /* 2.4576 MHz */
#endif /* AUTO_CLOCK */
#endif /* PC98 */

#ifndef PC98
	writertc(RTC_STATUSA, rtc_statusa);
	writertc(RTC_STATUSB, RTCSB_24HR);
#endif

#ifndef PC98
	set_timer_freq(timer_freq, hz);
	freq = calibrate_clocks();
#ifdef CLK_CALIBRATION_LOOP
	if (bootverbose) {
		printf(
		"Press a key on the console to abort clock calibration\n");
		while (cncheckc() == -1)
			calibrate_clocks();
	}
#endif

	/*
	 * Use the calibrated i8254 frequency if it seems reasonable.
	 * Otherwise use the default, and don't use the calibrated i586
	 * frequency.
	 */
	delta = freq > timer_freq ? freq - timer_freq : timer_freq - freq;
	if (delta < timer_freq / 100) {
#ifndef CLK_USE_I8254_CALIBRATION
		if (bootverbose)
			printf(
"CLK_USE_I8254_CALIBRATION not specified - using default frequency\n");
		freq = timer_freq;
#endif
		timer_freq = freq;
	} else {
		if (bootverbose)
			printf(
		    "%d Hz differs from default of %d Hz by more than 1%%\n",
			       freq, timer_freq);
#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
		i586_ctr_freq = 0;
#endif
	}
#endif

	set_timer_freq(timer_freq, hz);

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
#ifndef CLK_USE_I586_CALIBRATION
	if (i586_ctr_freq != 0) {
		if (bootverbose)
			printf(
"CLK_USE_I586_CALIBRATION not specified - using old calibration method\n");
		i586_ctr_freq = 0;
	}
#endif
	if (i586_ctr_freq == 0 &&
	    (cpu_class == CPUCLASS_586 || cpu_class == CPUCLASS_686)) {
		/*
		 * Calibration of the i586 clock relative to the mc146818A
		 * clock failed.  Do a less accurate calibration relative
		 * to the i8254 clock.
		 */
		wrmsr(0x10, 0LL);	/* XXX */
		DELAY(1000000);
		set_i586_ctr_freq((u_int)rdtsc(), timer_freq);
#ifdef CLK_USE_I586_CALIBRATION
		if (bootverbose)
			printf("i586 clock: %u Hz\n", i586_ctr_freq);
#endif
	}
#endif
}

#ifdef PC98
static void
rtc_serialcombit(int i)
{
	outb(IO_RTC, ((i&0x01)<<5)|0x07);
	DELAY(1);
	outb(IO_RTC, ((i&0x01)<<5)|0x17);
	DELAY(1);
	outb(IO_RTC, ((i&0x01)<<5)|0x07);
	DELAY(1);
}

static void
rtc_serialcom(int i)
{
	rtc_serialcombit(i&0x01);
	rtc_serialcombit((i&0x02)>>1);
	rtc_serialcombit((i&0x04)>>2);
	rtc_serialcombit((i&0x08)>>3);
	outb(IO_RTC, 0x07);
	DELAY(1);
	outb(IO_RTC, 0x0f);
	DELAY(1);
	outb(IO_RTC, 0x07);
 	DELAY(1);
}

static void
rtc_outb(int val)
{
	int s;
	int sa = 0;

	for (s=0;s<8;s++) {
	    sa = ((val >> s) & 0x01) ? 0x27 : 0x07;
	    outb(IO_RTC, sa);		/* set DI & CLK 0 */
	    DELAY(1);
	    outb(IO_RTC, sa | 0x10);	/* CLK 1 */
	    DELAY(1);
	}
	outb(IO_RTC, sa & 0xef);	/* CLK 0 */
}

static int
rtc_inb(void)
{
	int s;
	int sa = 0;

	for (s=0;s<8;s++) {
	    sa |= ((inb(0x33) & 0x01) << s);
	    outb(IO_RTC, 0x17);	/* CLK 1 */
	    DELAY(1);
	    outb(IO_RTC, 0x07);	/* CLK 0 */
	    DELAY(2);
	}
	return sa;
}
#endif /* PC-98 */

/*
 * Initialize the time of day register,	based on the time base which is, e.g.
 * from	a filesystem.
 */
void
inittodr(time_t base)
{
	unsigned long	sec, days;
	int		yd;
	int		year, month;
	int		y, m, s;
#ifdef PC98
	int		second, min, hour;
#endif

	s = splclock();
	time.tv_sec  = base;
	time.tv_usec = 0;
	splx(s);

#ifdef PC98
	rtc_serialcom(0x03);	/* Time Read */
	rtc_serialcom(0x01);	/* Register shift command. */
	DELAY(20);

	second = bcd2bin(rtc_inb() & 0xff);	/* sec */
	min = bcd2bin(rtc_inb() & 0xff);	/* min */
	hour = bcd2bin(rtc_inb() & 0xff);	/* hour */
	days = bcd2bin(rtc_inb() & 0xff) - 1;	/* date */

	month = (rtc_inb() >> 4) & 0x0f;	/* month */
	for (m = 1; m <	month; m++)
		days +=	daysinmonth[m-1];
	year = bcd2bin(rtc_inb() & 0xff) + 1900;	/* year */
	/* 2000 year problem */
	if (year < 1995)
		year += 100;
	if (year < 1970)
		goto wrong_time;
	for (y = 1970; y < year; y++)
		days +=	DAYSPERYEAR + LEAPYEAR(y);
	if ((month > 2)	&& LEAPYEAR(year))
		days ++;
	sec = ((( days * 24 +
		  hour) * 60 +
		  min) * 60 +
		  second);
	/* sec now contains the	number of seconds, since Jan 1 1970,
	   in the local	time zone */
#else	/* IBM-PC */
	/* Look	if we have a RTC present and the time is valid */
	if (!(rtcin(RTC_STATUSD) & RTCSD_PWR))
		goto wrong_time;

	/* wait	for time update	to complete */
	/* If RTCSA_TUP	is zero, we have at least 244us	before next update */
	while (rtcin(RTC_STATUSA) & RTCSA_TUP);

	days = 0;
#ifdef USE_RTC_CENTURY
	year = readrtc(RTC_YEAR) + readrtc(RTC_CENTURY)	* 100;
#else
	year = readrtc(RTC_YEAR) + 1900;
	if (year < 1970)
		year += 100;
#endif
	if (year < 1970)
		goto wrong_time;
	month =	readrtc(RTC_MONTH);
	for (m = 1; m <	month; m++)
		days +=	daysinmonth[m-1];
	if ((month > 2)	&& LEAPYEAR(year))
		days ++;
	days +=	readrtc(RTC_DAY) - 1;
	yd = days;
	for (y = 1970; y < year; y++)
		days +=	DAYSPERYEAR + LEAPYEAR(y);
	sec = ((( days * 24 +
		  readrtc(RTC_HRS)) * 60 +
		  readrtc(RTC_MIN)) * 60 +
		  readrtc(RTC_SEC));
	/* sec now contains the	number of seconds, since Jan 1 1970,
	   in the local	time zone */
#endif

	sec += tz.tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	s = splclock();
	time.tv_sec = sec;
	splx(s);
	return;

wrong_time:
	printf("Invalid	time in	real time clock.\n");
	printf("Check and reset	the date immediately!\n");
}

/*
 * Write system	time back to RTC
 */
void
resettodr()
{
	unsigned long	tm;
	int		y, m, s;
#ifdef PC98
	int		wd;
#endif

	if (disable_rtc_set)
		return;

	s = splclock();
	tm = time.tv_sec;
	splx(s);

#ifdef PC98
	rtc_serialcom(0x01);	/* Register shift command. */

	/* Calculate local time	to put in RTC */

	tm -= tz.tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	rtc_outb(bin2bcd(tm%60)); tm /= 60;	/* Write back Seconds */
	rtc_outb(bin2bcd(tm%60)); tm /= 60;	/* Write back Minutes */
	rtc_outb(bin2bcd(tm%24)); tm /= 24;	/* Write back Hours   */

	/* We have now the days	since 01-01-1970 in tm */
	wd = (tm+4)%7;
	for (y = 1970, m = DAYSPERYEAR + LEAPYEAR(y);
	     tm >= m;
	     y++,      m = DAYSPERYEAR + LEAPYEAR(y))
	     tm -= m;

	/* Now we have the years in y and the day-of-the-year in tm */
	for (m = 0; ; m++) {
		int ml;

		ml = daysinmonth[m];
		if (m == 1 && LEAPYEAR(y))
			ml++;
		if (tm < ml)
			break;
		tm -= ml;
	}

	m++;
	rtc_outb(bin2bcd(tm+1));		/* Write back Day     */
	rtc_outb((m << 4) | wd);		/* Write back Month & Weekday  */
	rtc_outb(bin2bcd(y%100));		/* Write back Year    */

	rtc_serialcom(0x02);	/* Time set & Counter hold command. */
	rtc_serialcom(0x00);	/* Register hold command. */
#else
	/* Disable RTC updates and interrupts. */
	writertc(RTC_STATUSB, RTCSB_HALT | RTCSB_24HR);

	/* Calculate local time	to put in RTC */

	tm -= tz.tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	writertc(RTC_SEC, bin2bcd(tm%60)); tm /= 60;	/* Write back Seconds */
	writertc(RTC_MIN, bin2bcd(tm%60)); tm /= 60;	/* Write back Minutes */
	writertc(RTC_HRS, bin2bcd(tm%24)); tm /= 24;	/* Write back Hours   */

	/* We have now the days	since 01-01-1970 in tm */
	writertc(RTC_WDAY, (tm+4)%7);			/* Write back Weekday */
	for (y = 1970, m = DAYSPERYEAR + LEAPYEAR(y);
	     tm >= m;
	     y++,      m = DAYSPERYEAR + LEAPYEAR(y))
	     tm -= m;

	/* Now we have the years in y and the day-of-the-year in tm */
	writertc(RTC_YEAR, bin2bcd(y%100));		/* Write back Year    */
#ifdef USE_RTC_CENTURY
	writertc(RTC_CENTURY, bin2bcd(y/100));		/* ... and Century    */
#endif
	for (m = 0; ; m++) {
		int ml;

		ml = daysinmonth[m];
		if (m == 1 && LEAPYEAR(y))
			ml++;
		if (tm < ml)
			break;
		tm -= ml;
	}

	writertc(RTC_MONTH, bin2bcd(m + 1));            /* Write back Month   */
	writertc(RTC_DAY, bin2bcd(tm + 1));             /* Write back Month Day */

	/* Reenable RTC updates and interrupts. */
	writertc(RTC_STATUSB, rtc_statusb);
#endif
}

#ifdef APIC_IO

/* XXX FIXME: from icu.s: */
#ifdef NEW_STRATEGY

extern u_int	ivectors[];
extern u_int	vec[];
extern void	vec8254	__P((void));
extern u_int	Xintr8254;
extern u_int	mask8254;

#else /** NEW_STRATEGY */

#if !defined(APIC_PIN0_TIMER)
extern u_int	ivectors[];
extern u_int	vec[];
#endif

#ifndef APIC_PIN0_TIMER
extern void	vec8254	__P((void));
extern u_int	Xintr8254;
extern u_int	mask8254;
#endif /* APIC_PIN0_TIMER */

#endif /** NEW_STRATEGY */

#endif /* APIC_IO */

/*
 * Start both clocks running.
 */
void
cpu_initclocks()
{
#ifdef APIC_IO
	int x;
#endif /* APIC_IO */
#ifndef PC98
	int diag;

	if (statclock_disable) {
		/*
		 * The stat interrupt mask is different without the
		 * statistics clock.  Also, don't set the interrupt
		 * flag which would normally cause the RTC to generate
		 * interrupts.
		 */
		stat_imask = HWI_MASK | SWI_MASK;
		rtc_statusb = RTCSB_24HR;
	} else {
	        /* Setting stathz to nonzero early helps avoid races. */
		stathz = RTC_NOPROFRATE;
		profhz = RTC_PROFRATE;
        }
#endif

	/* Finish initializing 8253 timer 0. */
#ifdef APIC_IO

#ifdef NEW_STRATEGY
	/* 1st look for ExtInt on pin 0 */
	if (apic_int_type(0, 0) == 3) {
		/*
		 * Allow 8254 timer to INTerrupt 8259:
		 *  re-initialize master 8259:
		 *   reset; prog 4 bytes, single ICU, edge triggered
		 */
		outb(IO_ICU1, 0x13);
		outb(IO_ICU1 + 1, NRSVIDT);	/* start vector */
		outb(IO_ICU1 + 1, 0x00);	/* ignore slave */
		outb(IO_ICU1 + 1, 0x03);	/* auto EOI, 8086 */
		outb(IO_ICU1 + 1, 0xfe);	/* unmask INT0 */

		/* program IO APIC for type 3 INT on INT0 */
		if (ext_int_setup(0, 0) < 0)
			panic("8254 redirect via APIC pin0 impossible!");

		x = 0;
		/** if (bootverbose */
			printf("APIC_IO: routing 8254 via 8259 on pin 0\n");
	}

	/* failing that, look for 8254 on pin 2 */
	else if (isa_apic_pin(0) == 2) {
		x = 2;
		/** if (bootverbose */
			printf("APIC_IO: routing 8254 via pin 2\n");
	}

	/* better write that 8254 INT discover code... */
	else 
		panic("neither pin 0 or pin 2 works for 8254");

	/* setup the vectors for the chosen method */
	vec[x] = (u_int)vec8254;
	Xintr8254 = (u_int)ivectors[x];
	mask8254 = (1 << x);

	register_intr(/* irq */ x, /* XXX id */ 0, /* flags */ 0,
		      /* XXX */ (inthand2_t *)clkintr, &clk_imask,
		      /* unit */ 0);
	INTREN(mask8254);

#else /** NEW_STRATEGY */

#ifdef APIC_PIN0_TIMER
	/*
	 * Allow 8254 timer to INTerrupt 8259:
	 *  re-initialize master 8259:
	 *   reset; prog 4 bytes, single ICU, edge triggered
	 */
	outb(IO_ICU1, 0x13);
	outb(IO_ICU1 + 1, NRSVIDT);	/* start vector */
	outb(IO_ICU1 + 1, 0x00);	/* ignore slave */
	outb(IO_ICU1 + 1, 0x03);	/* auto EOI, 8086 */
	outb(IO_ICU1 + 1, 0xfe);	/* unmask INT0 */

	/* program IO APIC for type 3 INT on INT0 */
	if (ext_int_setup(0, 0) < 0)
		panic("8254 redirect via APIC pin0 impossible!");

	register_intr(/* irq */ 0, /* XXX id */ 0, /* flags */ 0,
		      /* XXX */ (inthand2_t *)clkintr, &clk_imask,
		      /* unit */ 0);
	INTREN(IRQ0);
#else /* APIC_PIN0_TIMER */
	/* 8254 is traditionally on ISA IRQ0 */
	if ((x = isa_apic_pin(0)) < 0) {
		/* bummer, attempt to redirect thru the 8259 */
		if (bootverbose)
			printf("APIC missing 8254 connection\n");

		/* allow 8254 timer to INTerrupt 8259 */
#ifdef TEST_ALTTIMER
		/*
		 * re-initialize master 8259:
		 * reset; prog 4 bytes, single ICU, edge triggered
		 */
		outb(IO_ICU1, 0x13);
		outb(IO_ICU1 + 1, NRSVIDT);	/* start vector */
		outb(IO_ICU1 + 1, 0x00);	/* ignore slave */
		outb(IO_ICU1 + 1, 0x03);	/* auto EOI, 8086 */

		outb(IO_ICU1 + 1, 0xfe);	/* unmask INT0 */
#else
		x = inb(IO_ICU1 + 1);		/* current mask in 8259 */
		x &= ~1;			/* clear 8254 timer mask */
		outb(IO_ICU1 + 1, x);		/* write new mask */
#endif /* TEST_ALTTIMER */

		/* program IO APIC for type 3 INT on INT0 */
		if (ext_int_setup(0, 0) < 0)
			panic("8254 redirect impossible!");
		x = 0;				/* 8259 is on 0 */
	}

	vec[x] = (u_int)vec8254;
	Xintr8254 = (u_int)ivectors[x];	/* XXX might need Xfastintr# */
	mask8254 = (1 << x);
	register_intr(/* irq */ x, /* XXX id */ 0, /* flags */ 0,
		      /* XXX */ (inthand2_t *)clkintr, &clk_imask,
		      /* unit */ 0);
	INTREN(mask8254);
#endif /* APIC_PIN0_TIMER */

#endif /** NEW_STRATEGY */

#else /* APIC_IO */
	register_intr(/* irq */ 0, /* XXX id */ 0, /* flags */ 0,
		      /* XXX */ (inthand2_t *)clkintr, &clk_imask,
		      /* unit */ 0);
	INTREN(IRQ0);
#endif /* APIC_IO */

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
	/*
	 * Finish setting up anti-jitter measures.
	 */
	if (i586_ctr_freq != 0)
		i586_ctr_bias = rdtsc();
#endif

#ifndef PC98
	/* Initialize RTC. */
	writertc(RTC_STATUSA, rtc_statusa);
	writertc(RTC_STATUSB, RTCSB_24HR);

	/* Don't bother enabling the statistics clock. */
	if (statclock_disable)
		return;
	diag = rtcin(RTC_DIAG);
	if (diag != 0)
		printf("RTC BIOS diagnostic error %b\n", diag, RTCDG_BITS);

#ifdef APIC_IO
	if (isa_apic_pin(8) != 8)
		panic("APIC RTC != 8");
#endif /* APIC_IO */
	register_intr(/* irq */ 8, /* XXX id */ 1, /* flags */ 0,
		      /* XXX */ (inthand2_t *)rtcintr, &stat_imask,
		      /* unit */ 0);
	INTREN(IRQ8);

	writertc(RTC_STATUSB, rtc_statusb);
#endif /* !PC98 */
}

void
setstatclockrate(int newhz)
{
#ifndef PC98
	if (newhz == RTC_PROFRATE)
		rtc_statusa = RTCSA_DIVIDER | RTCSA_PROF;
	else
		rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;
	writertc(RTC_STATUSA, rtc_statusa);
#endif
}

static int
sysctl_machdep_i8254_freq SYSCTL_HANDLER_ARGS
{
	int error;
	u_int freq;

	/*
	 * Use `i8254' instead of `timer' in external names because `timer'
	 * is is too generic.  Should use it everywhere.
	 */
	freq = timer_freq;
	error = sysctl_handle_opaque(oidp, &freq, sizeof freq, req);
	if (error == 0 && req->newptr != NULL) {
		if (timer0_state != 0)
			return (EBUSY);	/* too much trouble to handle */
		set_timer_freq(freq, hz);
#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
		set_i586_ctr_freq(i586_ctr_freq, timer_freq);
#endif
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, i8254_freq, CTLTYPE_INT | CTLFLAG_RW,
	    0, sizeof(u_int), sysctl_machdep_i8254_freq, "I", "");

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
static void
set_i586_ctr_freq(u_int i586_freq, u_int i8254_freq)
{
	u_int comultiplier, multiplier;
	u_long ef;

	if (i586_freq == 0) {
		i586_ctr_freq = i586_freq;
		return;
	}
	comultiplier = ((unsigned long long)i586_freq
			<< I586_CTR_COMULTIPLIER_SHIFT) / i8254_freq;
	multiplier = (1000000LL << I586_CTR_MULTIPLIER_SHIFT) / i586_freq;
	ef = read_eflags();
	disable_intr();
	i586_ctr_freq = i586_freq;
	i586_ctr_comultiplier = comultiplier;
	i586_ctr_multiplier = multiplier;
	write_eflags(ef);
}

static int
sysctl_machdep_i586_freq SYSCTL_HANDLER_ARGS
{
	int error;
	u_int freq;

	if (cpu_class != CPUCLASS_586 && cpu_class != CPUCLASS_686)
		return (EOPNOTSUPP);
	freq = i586_ctr_freq;
	error = sysctl_handle_opaque(oidp, &freq, sizeof freq, req);
	if (error == 0 && req->newptr != NULL)
		set_i586_ctr_freq(freq, timer_freq);
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, i586_freq, CTLTYPE_INT | CTLFLAG_RW,
	    0, sizeof(u_int), sysctl_machdep_i586_freq, "I", "");
#endif /* (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP) */
