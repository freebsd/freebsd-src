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
 * $FreeBSD$
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
#include "opt_isa.h"
#include "opt_mca.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/cons.h>
#include <sys/power.h>

#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#ifdef APIC_IO
#include <machine/segments.h>
#endif
#if defined(SMP) || defined(APIC_IO)
#include <machine/smp.h>
#endif /* SMP || APIC_IO */
#include <machine/specialreg.h>

#include <i386/isa/icu.h>
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <i386/isa/isa_device.h>
#ifdef DEV_ISA
#include <isa/isavar.h>
#endif
#include <i386/isa/timerreg.h>

#include <i386/isa/intr_machdep.h>

#ifdef APIC_IO
#include <i386/isa/intr_machdep.h>
/* The interrupt triggered by the 8254 (timer) chip */
int apic_8254_intr;
static u_long read_intr_count(int vec);
static void setup_8254_mixed_mode(void);
#endif

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y) (((u_int)(y) % 4 == 0) ? 1 : 0)
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
 * before the next timer interrupt.
 */
#define	TIMER0_MAX_FREQ		20000

int	adjkerntz;		/* local offset from GMT in seconds */
int	clkintr_pending;
int	disable_rtc_set;	/* disable resettodr() if != 0 */
int	pscnt = 1;
int	psdiv = 1;
int	statclock_disable;
#ifndef TIMER_FREQ
#define TIMER_FREQ   2457600
#endif
u_int	timer_freq = TIMER_FREQ;
int	timer0_max_count;
int	wall_cmos_clock;	/* wall CMOS clock assumed if != 0 */
struct mtx clock_lock;

static	int	beeping = 0;
static	const u_char daysinmonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
static	u_int	hardclock_max_count;
static	u_int32_t i8254_lastcount;
static	u_int32_t i8254_offset;
static	int	i8254_ticked;
/*
 * XXX new_function and timer_func should not handle clockframes, but
 * timer_func currently needs to hold hardclock to handle the
 * timer0_state == 0 case.  We should use inthand_add()/inthand_remove()
 * to switch between clkintr() and a slightly different timerintr().
 */
static	void	(*new_function)(struct clockframe *frame);
static	u_int	new_rate;
static	u_int	timer0_prescaler_count;

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

static	u_char	timer0_state;
static 	u_char	timer1_state;
static	u_char	timer2_state;
static	void	(*timer_func)(struct clockframe *frame) = hardclock;
static void rtc_serialcombit(int);
static void rtc_serialcom(int);
static int rtc_inb(void);
static void rtc_outb(int);

static	unsigned i8254_get_timecount(struct timecounter *tc);
static	void	set_timer_freq(u_int freq, int intr_freq);

static struct timecounter i8254_timecounter = {
	i8254_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"i8254"			/* name */
};

static void
clkintr(struct clockframe frame)
{

	if (timecounter->tc_get_timecount == i8254_get_timecount) {
		mtx_lock_spin(&clock_lock);
		if (i8254_ticked)
			i8254_ticked = 0;
		else {
			i8254_offset += timer0_max_count;
			i8254_lastcount = 0;
		}
		clkintr_pending = 0;
		mtx_unlock_spin(&clock_lock);
	}
	timer_func(&frame);
#ifdef SMP
	if (timer_func == hardclock)
		forward_hardclock();
#endif
	switch (timer0_state) {

	case RELEASED:
		break;

	case ACQUIRED:
		if ((timer0_prescaler_count += timer0_max_count)
		    >= hardclock_max_count) {
			timer0_prescaler_count -= hardclock_max_count;
			hardclock(&frame);
#ifdef SMP
			forward_hardclock();
#endif
		}
		break;

	case ACQUIRE_PENDING:
		mtx_lock_spin(&clock_lock);
		i8254_offset = i8254_get_timecount(NULL);
		i8254_lastcount = 0;
		timer0_max_count = TIMER_DIV(new_rate);
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
		outb(TIMER_CNTR0, timer0_max_count & 0xff);
		outb(TIMER_CNTR0, timer0_max_count >> 8);
		mtx_unlock_spin(&clock_lock);
		timer_func = new_function;
		timer0_state = ACQUIRED;
		break;

	case RELEASE_PENDING:
		if ((timer0_prescaler_count += timer0_max_count)
		    >= hardclock_max_count) {
			mtx_lock_spin(&clock_lock);
			i8254_offset = i8254_get_timecount(NULL);
			i8254_lastcount = 0;
			timer0_max_count = hardclock_max_count;
			outb(TIMER_MODE,
			     TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
			outb(TIMER_CNTR0, timer0_max_count & 0xff);
			outb(TIMER_CNTR0, timer0_max_count >> 8);
			mtx_unlock_spin(&clock_lock);
			timer0_prescaler_count = 0;
			timer_func = hardclock;
			timer0_state = RELEASED;
			hardclock(&frame);
#ifdef SMP
			forward_hardclock();
#endif
		}
		break;
	}
}

/*
 * The acquire and release functions must be called at ipl >= splclock().
 */
int
acquire_timer0(int rate, void (*function)(struct clockframe *frame))
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

int
release_timer1()
{

	if (timer1_state != ACQUIRED)
		return (-1);
	timer1_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL1 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
}

int
release_timer2()
{

	if (timer2_state != ACQUIRED)
		return (-1);
	timer2_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
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
	outb(IO_PPI, inb(IO_PPI)|0x08);	/* disable counter1 output to speaker */
	release_timer1();
	beeping = 0;
}

int
sysbeep(int pitch, int period)
{
	int x = splclock();

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
	splx(x);
	return (0);
}


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

static u_int
calibrate_clocks(void)
{
	int	timeout;
	u_int	count, prev_count, tot_count;
	u_short	sec, start_sec;

	if (bootverbose)
	        printf("Calibrating clock(s) ... ");
	/* Check ARTIC. */
	if (!(PC98_SYSTEM_PARAMETER(0x458) & 0x80) &&
	    !(PC98_SYSTEM_PARAMETER(0x45b) & 0x04))
		goto fail;
	timeout = 100000000;

	/* Read the ARTIC. */
	sec = inw(0x5e);

	/* Wait for the ARTIC to changes. */
	start_sec = sec;
	for (;;) {
		sec = inw(0x5e);
		if (sec != start_sec)
			break;
		if (--timeout == 0)
			goto fail;
	}
	prev_count = getit();
	if (prev_count == 0 || prev_count > timer0_max_count)
		goto fail;
	tot_count = 0;

	start_sec = sec;
	for (;;) {
		sec = inw(0x5e);
		count = getit();
		if (count == 0 || count > timer0_max_count)
			goto fail;
		if (count > prev_count)
			tot_count += prev_count - (count - timer0_max_count);
		else
			tot_count += prev_count - count;
		prev_count = count;
		if ((sec == start_sec + 1200) ||
		    (sec < start_sec &&
		        (u_int)sec + 0x10000 == (u_int)start_sec + 1200))
			break;
		if (--timeout == 0)
			goto fail;
	}

	if (bootverbose) {
	        printf("i8254 clock: %u Hz\n", tot_count);
	}
	return (tot_count);

fail:
	if (bootverbose)
	        printf("failed, using default i8254 clock of %u Hz\n",
		       timer_freq);
	return (timer_freq);
}

static void
set_timer_freq(u_int freq, int intr_freq)
{
	int new_timer0_max_count;

	mtx_lock_spin(&clock_lock);
	timer_freq = freq;
	new_timer0_max_count = hardclock_max_count = TIMER_DIV(intr_freq);
	if (new_timer0_max_count != timer0_max_count) {
		timer0_max_count = new_timer0_max_count;
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
		outb(TIMER_CNTR0, timer0_max_count & 0xff);
		outb(TIMER_CNTR0, timer0_max_count >> 8);
	}
	mtx_unlock_spin(&clock_lock);
}

static void
i8254_restore(void)
{

	mtx_lock_spin(&clock_lock);
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR0, timer0_max_count & 0xff);
	outb(TIMER_CNTR0, timer0_max_count >> 8);
	mtx_unlock_spin(&clock_lock);
}


/*
 * Restore all the timers non-atomically (XXX: should be atomically).
 *
 * This function is called from pmtimer_resume() to restore all the timers.
 * This should not be necessary, but there are broken laptops that do not
 * restore all the timers on resume.
 */
void
timer_restore(void)
{

	i8254_restore();		/* restore timer_freq and hz */
}

/*
 * Initialize 8254 timer 0 early so that it can be used in DELAY().
 * XXX initialization of other timers is unintentionally left blank.
 */
void
startrtclock()
{
	u_int delta, freq;

	findcpuspeed();
	if (pc98_machine_type & M_8M)
		timer_freq = 1996800L; /* 1.9968 MHz */
	else
		timer_freq = 2457600L; /* 2.4576 MHz */

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
	}

	set_timer_freq(timer_freq, hz);
	i8254_timecounter.tc_frequency = timer_freq;
	tc_init(&i8254_timecounter);

	init_TSC();
}

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

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(time_t base)
{
	unsigned long	sec, days;
	int		year, month;
	int		y, m, s;
	struct timespec ts;
	int		second, min, hour;

	if (base) {
		s = splclock();
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
		splx(s);
	}

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

	s = splhigh();

	sec += tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	y = time_second - sec;
	if (y <= -2 || y >= 2) {
		/* badly off, adjust it */
		ts.tv_sec = sec;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
	}
	splx(s);
	return;

wrong_time:
	printf("Invalid time in real time clock.\n");
	printf("Check and reset the date immediately!\n");
}

/*
 * Write system time back to RTC
 */
void
resettodr()
{
	unsigned long	tm;
	int		y, m, s;
	int		wd;

	if (disable_rtc_set)
		return;

	s = splclock();
	tm = time_second;
	splx(s);

	rtc_serialcom(0x01);	/* Register shift command. */

	/* Calculate local time	to put in RTC */

	tm -= tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	rtc_outb(bin2bcd(tm%60)); tm /= 60;	/* Write back Seconds */
	rtc_outb(bin2bcd(tm%60)); tm /= 60;	/* Write back Minutes */
	rtc_outb(bin2bcd(tm%24)); tm /= 24;	/* Write back Hours   */

	/* We have now the days	since 01-01-1970 in tm */
	wd = (tm + 4) % 7 + 1;			/* Write back Weekday */
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
}


/*
 * Start both clocks running.
 */
void
cpu_initclocks()
{
#ifdef APIC_IO
	int apic_8254_trial;
	void *clkdesc;
#endif /* APIC_IO */
	register_t crit;


	/* Finish initializing 8253 timer 0. */
#ifdef APIC_IO

	apic_8254_intr = isa_apic_irq(0);
	apic_8254_trial = 0;
	if (apic_8254_intr >= 0 ) {
		if (apic_int_type(0, 0) == 3)
			apic_8254_trial = 1;
	} else {
		/* look for ExtInt on pin 0 */
		if (apic_int_type(0, 0) == 3) {
			apic_8254_intr = apic_irq(0, 0);
			setup_8254_mixed_mode();
		} else 
			panic("APIC_IO: Cannot route 8254 interrupt to CPU");
	}

	inthand_add("clk", apic_8254_intr, (driver_intr_t *)clkintr, NULL,
	    INTR_TYPE_CLK | INTR_FAST, &clkdesc);
	crit = intr_disable();
	mtx_lock_spin(&icu_lock);
	INTREN(1 << apic_8254_intr);
	mtx_unlock_spin(&icu_lock);
	intr_restore(crit);

#else /* APIC_IO */

	/*
	 * XXX Check the priority of this interrupt handler.  I
	 * couldn't find anything suitable in the BSD/OS code (grog,
	 * 19 July 2000).
	 */
	inthand_add("clk", 0, (driver_intr_t *)clkintr, NULL,
	    INTR_TYPE_CLK | INTR_FAST, NULL);
	crit = intr_disable();
	mtx_lock_spin(&icu_lock);
	INTREN(IRQ0);
	mtx_unlock_spin(&icu_lock);
	intr_restore(crit);

#endif /* APIC_IO */



#ifdef APIC_IO
	if (apic_8254_trial) {

		printf("APIC_IO: Testing 8254 interrupt delivery\n");
		while (read_intr_count(8) < 6)
			;	/* nothing */
		if (read_intr_count(apic_8254_intr) < 3) {
			/* 
			 * The MP table is broken.
			 * The 8254 was not connected to the specified pin
			 * on the IO APIC.
			 * Workaround: Limited variant of mixed mode.
			 */

			crit = intr_disable();
			mtx_lock_spin(&icu_lock);
			INTRDIS(1 << apic_8254_intr);
			mtx_unlock_spin(&icu_lock);
			intr_restore(crit);
			inthand_remove(clkdesc);
			printf("APIC_IO: Broken MP table detected: "
			       "8254 is not connected to "
			       "IOAPIC #%d intpin %d\n",
			       int_to_apicintpin[apic_8254_intr].ioapic,
			       int_to_apicintpin[apic_8254_intr].int_pin);
			/* 
			 * Revoke current ISA IRQ 0 assignment and 
			 * configure a fallback interrupt routing from
			 * the 8254 Timer via the 8259 PIC to the
			 * an ExtInt interrupt line on IOAPIC #0 intpin 0.
			 * We reuse the low level interrupt handler number.
			 */
			if (apic_irq(0, 0) < 0) {
				revoke_apic_irq(apic_8254_intr);
				assign_apic_irq(0, 0, apic_8254_intr);
			}
			apic_8254_intr = apic_irq(0, 0);
			setup_8254_mixed_mode();
			inthand_add("clk", apic_8254_intr,
				    (driver_intr_t *)clkintr, NULL,
				    INTR_TYPE_CLK | INTR_FAST, NULL);
			crit = intr_disable();
			mtx_lock_spin(&icu_lock);
			INTREN(1 << apic_8254_intr);
			mtx_unlock_spin(&icu_lock);
			intr_restore(crit);
		}
		
	}
	if (apic_int_type(0, 0) != 3 ||
	    int_to_apicintpin[apic_8254_intr].ioapic != 0 ||
	    int_to_apicintpin[apic_8254_intr].int_pin != 0)
		printf("APIC_IO: routing 8254 via IOAPIC #%d intpin %d\n",
		       int_to_apicintpin[apic_8254_intr].ioapic,
		       int_to_apicintpin[apic_8254_intr].int_pin);
	else
		printf("APIC_IO: "
		       "routing 8254 via 8259 and IOAPIC #0 intpin 0\n");
#endif
	
	init_TSC_tc();
}

#ifdef APIC_IO
static u_long
read_intr_count(int vec)
{
	u_long *up;
	up = intr_countp[vec];
	if (up)
		return *up;
	return 0UL;
}

static void 
setup_8254_mixed_mode()
{
	/*
	 * Allow 8254 timer to INTerrupt 8259:
	 *  re-initialize master 8259:
	 *   reset; prog 4 bytes, single ICU, edge triggered
	 */
	outb(IO_ICU1, 0x13);
	outb(IO_ICU1 + 2, NRSVIDT);	/* start vector (unused) */
	outb(IO_ICU1 + 2, 0x00);	/* ignore slave */
	outb(IO_ICU1 + 2, 0x03);	/* auto EOI, 8086 */
	outb(IO_ICU1 + 2, 0xfe);	/* unmask INT0 */
	
	/* program IO APIC for type 3 INT on INT0 */
	if (ext_int_setup(0, 0) < 0)
		panic("8254 redirect via APIC pin0 impossible!");
}
#endif

void
cpu_startprofclock(void)
{
}

void
cpu_stopprofclock(void)
{
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
	freq = timer_freq;
	error = sysctl_handle_int(oidp, &freq, sizeof(freq), req);
	if (error == 0 && req->newptr != NULL) {
		if (timer0_state != RELEASED)
			return (EBUSY);	/* too much trouble to handle */
		set_timer_freq(freq, hz);
		i8254_timecounter.tc_frequency = freq;
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, i8254_freq, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_i8254_freq, "IU", "");

static unsigned
i8254_get_timecount(struct timecounter *tc)
{
	u_int count;
	u_int high, low;
	u_int eflags;

	eflags = read_eflags();
	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);
	count = timer0_max_count - ((high << 8) | low);
	if (count < i8254_lastcount ||
	    (!i8254_ticked && (clkintr_pending ||
	    ((count < 20 || (!(eflags & PSL_I) && count < timer0_max_count / 2u)) &&
#ifdef APIC_IO
#define	lapic_irr1	((volatile u_int *)&lapic)[0x210 / 4]	/* XXX XXX */
	    /* XXX this assumes that apic_8254_intr is < 24. */
	    (lapic_irr1 & (1 << apic_8254_intr))))
#else
	    (inb(IO_ICU1) & 1)))
#endif
	    )) {
		i8254_ticked = 1;
		i8254_offset += timer0_max_count;
	}
	i8254_lastcount = count;
	count += i8254_offset;
	mtx_unlock_spin(&clock_lock);
	return (count);
}

#ifdef DEV_ISA
/*
 * Attach to the ISA PnP descriptors for the timer and realtime clock.
 */
static struct isa_pnp_id attimer_ids[] = {
	{ 0x0001d041 /* PNP0100 */, "AT timer" },
	{ 0x000bd041 /* PNP0B00 */, "AT realtime clock" },
	{ 0 }
};

static int
attimer_probe(device_t dev)
{
	int result;
	
	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, attimer_ids)) <= 0)
		device_quiet(dev);
	return(result);
}

static int
attimer_attach(device_t dev)
{
	return(0);
}

static device_method_t attimer_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		attimer_probe),
	DEVMETHOD(device_attach,	attimer_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),	/* XXX stop statclock? */
	DEVMETHOD(device_resume,	bus_generic_resume),	/* XXX restart statclock? */
	{ 0, 0 }
};

static driver_t attimer_driver = {
	"attimer",
	attimer_methods,
	1,		/* no softc */
};

static devclass_t attimer_devclass;

DRIVER_MODULE(attimer, isa, attimer_driver, attimer_devclass, 0, 0);
#endif /* DEV_ISA */
