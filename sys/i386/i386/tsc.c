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
 *	$Id: clock.c,v 1.23 1994/10/04 13:59:44 ache Exp $
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/frame.h>
#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/rtc.h>
#include <i386/isa/timerreg.h>

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y) ((u_int)(y) % 4 == 0)
#define DAYSPERYEAR   (31+28+31+30+31+30+31+31+30+31+30+31)

/* X-tals being what they are, it's nice to be able to fudge this one... */
/* Note, the name changed here from XTALSPEED to TIMER_FREQ rgrimes 4/26/93 */
#ifndef TIMER_FREQ
#define	TIMER_FREQ	1193182	/* XXX - should be in isa.h */
#endif
#define TIMER_DIV(x) ((TIMER_FREQ+(x)/2)/(x))

static	int beeping;
int 	timer0_divisor = TIMER_DIV(100);	/* XXX should be hz */
u_int 	timer0_prescale;
int	adjkerntz = 0;	/* offset from CMOS clock */
int	disable_rtc_set	= 0;	/* disable resettodr() if != 0 */
static 	char timer0_state = 0, timer2_state = 0;
static 	char timer0_reprogram = 0;
static 	void (*timer_func)() = hardclock;
static 	void (*new_function)();
static 	u_int new_rate;
static 	u_int hardclock_divisor;
static	const u_char daysinmonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
static 	u_char rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;

#ifdef I586_CPU
int pentium_mhz = 0;
#endif

#if 0
void
clkintr(struct clockframe frame)
{
	hardclock(&frame);
}
#else
void
clkintr(struct clockframe frame)
{
	timer_func(&frame);
	switch (timer0_state) {
	case 0:
		break;
	case 1:
		if ((timer0_prescale+=timer0_divisor) >= hardclock_divisor) {
			hardclock(&frame);
			timer0_prescale = 0;
		}
		break;
	case 2:
		disable_intr();
		outb(TIMER_MODE, TIMER_SEL0|TIMER_RATEGEN|TIMER_16BIT);
		outb(TIMER_CNTR0, TIMER_DIV(new_rate)%256);
		outb(TIMER_CNTR0, TIMER_DIV(new_rate)/256);
		enable_intr();
		timer0_divisor = TIMER_DIV(new_rate);
		timer0_prescale = 0;
		timer_func = new_function;
		timer0_state = 1;
		break;
	case 3:
		if ((timer0_prescale+=timer0_divisor) >= hardclock_divisor) {
			hardclock(&frame);
			disable_intr();
			outb(TIMER_MODE, TIMER_SEL0|TIMER_RATEGEN|TIMER_16BIT);
			outb(TIMER_CNTR0, TIMER_DIV(hz)%256);
			outb(TIMER_CNTR0, TIMER_DIV(hz)/256);
			enable_intr();
			timer0_divisor = TIMER_DIV(hz);
			timer0_prescale = 0;
			timer_func = hardclock;;
			timer0_state = 0;
		}
		break;
	}
}
#endif

int
acquire_timer0(int rate, void (*function)() )
{
	if (timer0_state || !function) 	
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
void
rtcintr(struct clockframe frame)
{
	u_char stat;
	stat = rtcin(RTC_INTR);
	if(stat & RTCIR_PERIOD) {
		statclock(&frame);
	}
}

#ifdef DEBUG
void
printrtc(void)
{
	outb(IO_RTC, RTC_STATUSA);
	printf("RTC status A = %x", inb(IO_RTC+1));
	outb(IO_RTC, RTC_STATUSB);
	printf(", B = %x", inb(IO_RTC+1));
	outb(IO_RTC, RTC_INTR);
	printf(", C = %x\n", inb(IO_RTC+1));
}
#endif

static int
getit() 
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

#ifdef I586_CPU
static long long cycles_per_sec = 0;

/*
 * Figure out how fast the cyclecounter runs.  This must be run with
 * clock interrupts disabled, but with the timer/counter programmed
 * and running.
 */
void
calibrate_cyclecounter(void)
{
	volatile long edx, eax, lasteax, lastedx;

	__asm __volatile(".byte 0x0f, 0x31" : "=a"(lasteax), "=d"(lastedx) : );
	DELAY(1000000);
	__asm __volatile(".byte 0x0f, 0x31" : "=a"(eax), "=d"(edx) : );

	/*
	 * This assumes that you will never have a clock rate higher
	 * than 4GHz, probably a good assumption.
	 */
	cycles_per_sec = (long long)edx + eax;
	cycles_per_sec -= (long long)lastedx + lasteax;
	pentium_mhz = ((long)cycles_per_sec + 500000) / 1000000; /* round up */
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
	prev_tick = getit(0, 0);
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
		tick = getit(0, 0);
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		if (tick > prev_tick)
			ticks_left -= prev_tick - (tick - timer0_divisor);
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
static int
bcd2int(int bcd)
{
	return(bcd/16 *	10 + bcd%16);
}

static int
int2bcd(int dez)
{
	return(dez/10 *	16 + dez%10);
}

static void
writertc(int port, int val)
{
	outb(IO_RTC, port);
	outb(IO_RTC+1, val);
}

static int
readrtc(int port)
{
	return(bcd2int(rtcin(port)));
}

void
startrtclock() 
{
	int s;

	/* initialize 8253 clock */
	outb(TIMER_MODE, TIMER_SEL0|TIMER_RATEGEN|TIMER_16BIT);

	/* Correct rounding will buy us a better precision in timekeeping */
	outb (IO_TIMER1, TIMER_DIV(hz)%256);
	outb (IO_TIMER1, TIMER_DIV(hz)/256);
	timer0_divisor = hardclock_divisor = TIMER_DIV(hz);

	/* initialize brain-dead battery powered clock */
	outb (IO_RTC, RTC_STATUSA);
	outb (IO_RTC+1, rtc_statusa);
	outb (IO_RTC, RTC_STATUSB);
	outb (IO_RTC+1, RTCSB_24HR);
	outb (IO_RTC, RTC_DIAG);
	if (s = inb (IO_RTC+1))
		printf("RTC BIOS diagnostic error %b\n", s, RTCDG_BITS);
	writertc(RTC_DIAG, 0);
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
	if (rtcin(RTC_STATUSD) != RTCSD_PWR)
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

	sec += tz.tz_minuteswest * 60;

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
void resettodr()
{
	unsigned long	tm;
	int		y, m, fd, r, s;

	if (disable_rtc_set)
		return;

	s = splclock();
	tm = time.tv_sec;
	splx(s);

	/* First, disable clock	updates	*/
	writertc(RTC_STATUSB, RTCSB_HALT | RTCSB_24HR);

	/* Calculate local time	to put in RTC */

	tm -= tz.tz_minuteswest * 60 + adjkerntz;

	writertc(RTC_SEC, int2bcd(tm%60)); tm /= 60;	/* Write back Seconds */
	writertc(RTC_MIN, int2bcd(tm%60)); tm /= 60;	/* Write back Minutes */
	writertc(RTC_HRS, int2bcd(tm%24)); tm /= 24;	/* Write back Hours   */

	/* We have now the days	since 01-01-1970 in tm */
	writertc(RTC_WDAY, (tm+4)%7);			/* Write back Weekday */
	for (y=1970;; y++)
		if ((tm	- DAYSPERYEAR -	LEAPYEAR(y)) > tm)
			break;
		else
			tm -= DAYSPERYEAR + LEAPYEAR(y);

	/* Now we have the years in y and the day-of-the-year in tm */
	writertc(RTC_YEAR, int2bcd(y%100));		/* Write back Year    */
#ifdef USE_RTC_CENTURY
	writertc(RTC_CENTURY, int2bcd(y/100));		/* ... and Century    */
#endif
	if (LEAPYEAR(y)	&& (tm >= 31+29))
		tm--;					/* Subtract Feb-29 */
	for (m=1;; m++)
		if (tm - daysinmonth[m-1] > tm)
			break;
		else
			tm -= daysinmonth[m-1];

	writertc(RTC_MONTH, int2bcd(m));		/* Write back Month   */
	writertc(RTC_DAY, int2bcd(tm+1));		/* Write back Day     */

	/* enable time updates */
	writertc(RTC_STATUSB, RTCSB_PINTR | RTCSB_24HR);
}

#ifdef garbage
/*
 * Initialze the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
test_inittodr(time_t base)
{

	outb(IO_RTC,9); /* year    */
	printf("%d ",bcd(inb(IO_RTC+1)));
	outb(IO_RTC,8); /* month   */
	printf("%d ",bcd(inb(IO_RTC+1)));
	outb(IO_RTC,7); /* day     */
	printf("%d ",bcd(inb(IO_RTC+1)));
	outb(IO_RTC,4); /* hour    */
	printf("%d ",bcd(inb(IO_RTC+1)));
	outb(IO_RTC,2); /* minutes */
	printf("%d ",bcd(inb(IO_RTC+1)));
	outb(IO_RTC,0); /* seconds */
	printf("%d\n",bcd(inb(IO_RTC+1)));

	time.tv_sec = base;
}
#endif

/*
 * Wire clock interrupt in.
 */
void
enablertclock() 
{
	register_intr(/* irq */ 0, /* XXX id */ 0, /* flags */ 0, clkintr,
		      HWI_MASK | SWI_MASK, /* unit */ 0);
	INTREN(IRQ0);
	register_intr(/* irq */ 8, /* XXX id */ 1, /* flags */ 0, rtcintr,
		      SWI_CLOCK_MASK, /* unit */ 0);
	INTREN(IRQ8);
	outb(IO_RTC, RTC_STATUSB);
	outb(IO_RTC+1, RTCSB_PINTR | RTCSB_24HR);
}

void
cpu_initclocks()
{
	stathz = RTC_NOPROFRATE;
	profhz = RTC_PROFRATE;
	enablertclock();
}

void
setstatclockrate(int newhz)
{
	if(newhz == RTC_PROFRATE) {
		rtc_statusa = RTCSA_DIVIDER | RTCSA_PROF;
	} else {
		rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;
	}
	outb(IO_RTC, RTC_STATUSA);
	outb(IO_RTC+1, rtc_statusa);
}
