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
 *	$Id: clock.c,v 1.53 1996/03/23 21:36:03 nate Exp $
 */

/*
 * inittodr, settodr and support routines written
 * by Christoph Robitschko <chmr@edvz.tu-graz.ac.at>
 *
 * reintroduced and updated by Chris Stenton <chris@gnome.co.uk> 8/10/94
 */

/*
 * Primitive clock interrupt routines.
 */
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/clock.h>
#include <machine/frame.h>
#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/rtc.h>
#include <i386/isa/timerreg.h>

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y) ((u_int)(y) % 4 == 0)
#define DAYSPERYEAR   (31+28+31+30+31+30+31+31+30+31+30+31)

/* X-tals being what they are, it's nice to be able to fudge this one... */
#ifndef TIMER_FREQ
#define	TIMER_FREQ	1193182	/* XXX - should be in isa.h */
#endif
#define TIMER_DIV(x) ((TIMER_FREQ+(x)/2)/(x))

/*
 * Time in timer cycles that it takes for microtime() to disable interrupts
 * and latch the count.  microtime() currently uses "cli; outb ..." so it
 * normally takes less than 2 timer cycles.  Add a few for cache misses.
 * Add a few more to allow for latency in bogus calls to microtime() with
 * interrupts already disabled.
 */
#define	TIMER0_LATCH_COUNT	20

/*
 * Minimum maximum count that we are willing to program into timer0.
 * Must be large enough to guarantee that the timer interrupt handler
 * returns before the next timer interrupt.  Must be larger than
 * TIMER0_LATCH_COUNT so that we don't have to worry about underflow in
 * the calculation of timer0_overflow_threshold.
 */
#define	TIMER0_MIN_MAX_COUNT	TIMER_DIV(20000)

int	adjkerntz = 0;		/* offset from CMOS clock */
int	disable_rtc_set	= 0;	/* disable resettodr() if != 0 */
u_int	idelayed;
#if defined(I586_CPU) || defined(I686_CPU)
unsigned	i586_ctr_rate;
long long	i586_ctr_bias;
long long	i586_last_tick;
unsigned long	i586_avg_tick;
#endif
u_int	stat_imask = SWI_CLOCK_MASK;
int 	timer0_max_count;
u_int 	timer0_overflow_threshold;
u_int 	timer0_prescaler_count;

static	int	beeping = 0;
static	u_int	clk_imask = HWI_MASK | SWI_MASK;
static	const u_char daysinmonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
static 	u_int	hardclock_max_count;
/*
 * XXX new_function and timer_func should not handle clockframes, but
 * timer_func currently needs to hold hardclock to handle the
 * timer0_state == 0 case.  We should use register_intr()/unregister_intr()
 * to switch between clkintr() and a slightly different timerintr().
 * This will require locking when acquiring and releasing timer0 - the
 * current (nonexistent) locking doesn't seem to be adequate even now.
 */
static 	void	(*new_function) __P((struct clockframe *frame));
static 	u_int	new_rate;
static	u_char	rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;
static 	char	timer0_state = 0;
static	char	timer2_state = 0;
static 	void	(*timer_func) __P((struct clockframe *frame)) = hardclock;

#if 0
void
clkintr(struct clockframe frame)
{
	hardclock(&frame);
	setdelayed();
}
#else
static void
clkintr(struct clockframe frame)
{
	timer_func(&frame);
	switch (timer0_state) {
	case 0:
		setdelayed();
		break;
	case 1:
		if ((timer0_prescaler_count += timer0_max_count)
		    >= hardclock_max_count) {
			hardclock(&frame);
			setdelayed();
			timer0_prescaler_count -= hardclock_max_count;
		}
		break;
	case 2:
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
		timer0_state = 1;
		break;
	case 3:
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
			time.tv_usec += (27645 *
				(timer0_prescaler_count - hardclock_max_count))
				>> 15;
			if (time.tv_usec >= 1000000)
				time.tv_usec -= 1000000;
			timer0_prescaler_count = 0;
			timer_func = hardclock;;
			timer0_state = 0;
		}
		break;
	}
}
#endif

int
acquire_timer0(int rate, void (*function) __P((struct clockframe *frame)))
{
	if (timer0_state || TIMER_DIV(rate) < TIMER0_MIN_MAX_COUNT ||
	    !function)
		return -1;
	new_function = function;
	new_rate = rate;
	timer0_state = 2;
	return 0;
}

int
acquire_timer2(int mode)
{
	if (timer2_state)
		return -1;
	timer2_state = 1;
	outb(TIMER_MODE, TIMER_SEL2 | (mode &0x3f));
	return 0;
}

int
release_timer0()
{
	if (!timer0_state)
		return -1;
	timer0_state = 3;
	return 0;
}

int
release_timer2()
{
	if (!timer2_state)
		return -1;
	timer2_state = 0;
	outb(TIMER_MODE, TIMER_SEL2|TIMER_SQWAVE|TIMER_16BIT);
	return 0;
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
 */
static void
rtcintr(struct clockframe frame)
{
	u_char stat;
	stat = rtcin(RTC_INTR);
	if(stat & RTCIR_PERIOD) {
		statclock(&frame);
	}
}

#ifdef DDB
static void
DDB_printrtc(void)
{
	printf("%02x/%02x/%02x %02x:%02x:%02x, A = %02x, B = %02x, C = %02x\n",
	       rtcin(RTC_YEAR), rtcin(RTC_MONTH), rtcin(RTC_DAY),
	       rtcin(RTC_HRS), rtcin(RTC_MIN), rtcin(RTC_SEC),
	       rtcin(RTC_STATUSA), rtcin(RTC_STATUSB), rtcin(RTC_INTR));
}
#endif

static int
getit(void)
{
	int high, low;

	disable_intr();
	/* select timer0 and latch counter value */
	outb(TIMER_MODE, TIMER_SEL0);
	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);
	enable_intr();
	return ((high << 8) | low);
}

#if defined(I586_CPU) || defined(I686_CPU)
/*
 * Figure out how fast the cyclecounter runs.  This must be run with
 * clock interrupts disabled, but with the timer/counter programmed
 * and running.
 */
void
calibrate_cyclecounter(void)
{
	/*
	 * Don't need volatile; should always use unsigned if 2's
	 * complement arithmetic is desired.
	 */
	unsigned long long count;

#define howlong 131072UL
	__asm __volatile(".byte 0x0f, 0x30" : : "A"(0LL), "c" (0x10));
	DELAY(howlong);
	__asm __volatile(".byte 0xf,0x31" : "=A" (count));

	i586_ctr_rate = (count << I586_CTR_RATE_SHIFT) / howlong;
#undef howlong
}
#endif

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (TIMER_FREQ / hz)
 * Note: timer had better have been programmed before this is first used!
 */
void
DELAY(int n)
{
	int prev_tick, tick, ticks_left, sec, usec;

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
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.  Guess the initial overhead is 20 usec (on most systems it
	 * takes about 1.5 usec for each of the i/o's in getit().  The loop
	 * takes about 6 usec on a 486/33 and 13 usec on a 386/20.  The
	 * multiplications and divisions to scale the count take a while).
	 */
	prev_tick = getit();
	n -= 20;
	/*
	 * Calculate (n * (TIMER_FREQ / 1e6)) without using floating point
	 * and without any avoidable overflows.
	 */
	sec = n / 1000000;
	usec = n - sec * 1000000;
	ticks_left = sec * TIMER_FREQ
		     + usec * (TIMER_FREQ / 1000000)
		     + usec * ((TIMER_FREQ % 1000000) / 1000) / 1000
		     + usec * (TIMER_FREQ % 1000) / 1000000;

	while (ticks_left > 0) {
		tick = getit();
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		if (tick > prev_tick)
			ticks_left -= prev_tick - (tick - timer0_max_count);
		else
			ticks_left -= prev_tick - tick;
		prev_tick = tick;
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
	outb(IO_PPI, inb(IO_PPI)&0xFC);	/* disable counter2 output to speaker */
	release_timer2();
	beeping = 0;
}

int
sysbeep(int pitch, int period)
{

	if (acquire_timer2(TIMER_SQWAVE|TIMER_16BIT))
		return -1;
	disable_intr();
	outb(TIMER_CNTR2, pitch);
	outb(TIMER_CNTR2, (pitch>>8));
	enable_intr();
	if (!beeping) {
	outb(IO_PPI, inb(IO_PPI) | 3);	/* enable counter2 output to speaker */
		beeping = period;
		timeout(sysbeepstop, (void *)NULL, period);
	}
	return 0;
}

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

/*
 * Initialize 8253 timer 0 early so that it can be used in DELAY().
 * XXX initialization of other timers is unintentionally left blank.
 */
void
startrtclock()
{
	timer0_max_count = hardclock_max_count = TIMER_DIV(hz);
	timer0_overflow_threshold = timer0_max_count - TIMER0_LATCH_COUNT;
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR0, timer0_max_count & 0xff);
	outb(TIMER_CNTR0, timer0_max_count >> 8);
}

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

	s = splclock();
	time.tv_sec  = base;
	time.tv_usec = 0;
	splx(s);

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

	sec += tz.tz_minuteswest * 60 + adjkerntz;

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

	if (disable_rtc_set)
		return;

	s = splclock();
	tm = time.tv_sec;
	splx(s);

	/* Disable RTC updates and interrupts. */
	writertc(RTC_STATUSB, RTCSB_HALT | RTCSB_24HR);

	/* Calculate local time	to put in RTC */

	tm -= tz.tz_minuteswest * 60 + adjkerntz;

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
	writertc(RTC_STATUSB, RTCSB_24HR | RTCSB_PINTR);
}

/*
 * Start both clocks running.
 */
void
cpu_initclocks()
{
	int diag;

	stathz = RTC_NOPROFRATE;
	profhz = RTC_PROFRATE;

	/* Finish initializing 8253 timer 0. */
	register_intr(/* irq */ 0, /* XXX id */ 0, /* flags */ 0,
		      /* XXX */ (inthand2_t *)clkintr, &clk_imask,
		      /* unit */ 0);
	INTREN(IRQ0);
#if defined(I586_CPU) || defined(I686_CPU)
	/*
	 * Finish setting up anti-jitter measures.
	 */
	if (i586_ctr_rate) {
		I586_CYCLECTR(i586_last_tick);
		i586_ctr_bias = i586_last_tick;
	}
#endif

	/* Initialize RTC. */
	writertc(RTC_STATUSA, rtc_statusa);
	writertc(RTC_STATUSB, RTCSB_24HR);
	diag = rtcin(RTC_DIAG);
	if (diag != 0)
		printf("RTC BIOS diagnostic error %b\n", diag, RTCDG_BITS);
	register_intr(/* irq */ 8, /* XXX id */ 1, /* flags */ 0,
		      /* XXX */ (inthand2_t *)rtcintr, &stat_imask,
		      /* unit */ 0);
	INTREN(IRQ8);
	writertc(RTC_STATUSB, RTCSB_24HR | RTCSB_PINTR);
}

void
setstatclockrate(int newhz)
{
	if (newhz == RTC_PROFRATE)
		rtc_statusa = RTCSA_DIVIDER | RTCSA_PROF;
	else
		rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;
	writertc(RTC_STATUSA, rtc_statusa);
}
