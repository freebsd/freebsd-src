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
 *	$Id: clock.c,v 1.15 1994/08/18 05:09:21 davidg Exp $
 */

/*
 * Primitive clock interrupt routines.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/segments.h>
#include <machine/frame.h>
#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/rtc.h>
#include <i386/isa/timerreg.h>
#include <machine/cpu.h>

/* X-tals being what they are, it's nice to be able to fudge this one... */
/* Note, the name changed here from XTALSPEED to TIMER_FREQ rgrimes 4/26/93 */
#ifndef TIMER_FREQ
#define	TIMER_FREQ	1193182	/* XXX - should be in isa.h */
#endif
#define TIMER_DIV(x) ((TIMER_FREQ+(x)/2)/(x))

static 	int beeping;
int 	timer0_divisor = TIMER_DIV(100);	/* XXX should be hz */
u_int 	timer0_prescale;
static 	char timer0_state = 0, timer2_state = 0;
static 	char timer0_reprogram = 0;
static 	void (*timer_func)() = hardclock;
static 	void (*new_function)();
static 	u_int new_rate;
static 	u_int hardclock_divisor;

#ifdef I586_CPU
int pentium_mhz = 0;
#endif

void
clkintr(frame)
	struct clockframe frame;
{
#ifdef I586_CPU
	/*
	 * This resets the CPU cycle counter to zero, to make our
	 * job easier in microtime().  Some fancy ifdefs could speed
	 * this up for Pentium-only kernels.
	 * We want this to be done as close as possible to the actual
	 * timer incrementing in hardclock(), because there is a window
	 * between the two where the value is no longer valid.  Experimentation
	 * may reveal a good precompensation to apply in microtime().
	 */
	if(pentium_mhz) {
		__asm __volatile("movl $0x10,%%ecx\n"
				 "xorl %%eax,%%eax\n"
				 "movl %%eax,%%edx\n"
				 ".byte 0x0f, 0x30\n"
				 "#%0%1"
				 : "=m"(frame)	/* no outputs */
				 : "b"(&frame) /* fake input */
				 : "ax", "cx", "dx");
	}
#endif
	hardclock(&frame);
}

static u_char rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;

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

#if 0
void
timerintr(struct clockframe frame)
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
	int counter_limit, prev_tick, tick, ticks_left, sec, usec;

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
sysbeepstop()
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
		timeout(sysbeepstop, 0, period);
	}
	return 0;
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
}


/* convert 2 digit BCD number */
int
bcd(int i)
{
	return ((i/16)*10 + (i%16));
}


/* convert years to seconds (from 1970) */
unsigned long
ytos(int y)
{
	int i;
	unsigned long ret;

	ret = 0;
	for(i = 1970; i < y; i++) {
		if (i % 4) ret += 365*24*60*60;
		else ret += 366*24*60*60;
	}
	return ret;
}


/* convert months to seconds */
unsigned long
mtos(int m, int leap)
{
	int i;
	unsigned long ret;

	ret = 0;
	for(i=1; i<m; i++) {
		switch(i){
		case 1: case 3: case 5: case 7: case 8: case 10: case 12:
			ret += 31*24*60*60; break;
		case 4: case 6: case 9: case 11:
			ret += 30*24*60*60; break;
		case 2:
			if (leap) ret += 29*24*60*60;
			else ret += 28*24*60*60;
		}
	}
	return ret;
}


/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(time_t base)
{
	unsigned long sec;
	int leap, day_week, t, yd;
	int sa,s;

	/* do we have a realtime clock present? (otherwise we loop below) */
	sa = rtcin(RTC_STATUSA);
	if (sa == 0xff || sa == 0) return;

	/* ready for a read? */
	while ((sa&RTCSA_TUP) == RTCSA_TUP)
		sa = rtcin(RTC_STATUSA);

	sec = bcd(rtcin(RTC_YEAR)) + 1900;
	if (sec < 1970)
		sec += 100;

	leap = !(sec % 4); sec = ytos(sec); 			/* year    */
	yd = mtos(bcd(rtcin(RTC_MONTH)),leap); sec+=yd;		/* month   */
	t = (bcd(rtcin(RTC_DAY))-1) * 24*60*60; sec+=t; yd+=t;	/* date    */
	day_week = rtcin(RTC_WDAY);				/* day     */
	sec += bcd(rtcin(RTC_HRS)) * 60*60;			/* hour    */
	sec += bcd(rtcin(RTC_MIN)) * 60;			/* minutes */
	sec += bcd(rtcin(RTC_SEC));				/* seconds */
	sec += tz.tz_minuteswest * 60;
	time.tv_sec = sec;
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


/*
 * Delay for some number of milliseconds.
 */
void
spinwait(int millisecs)
{
	DELAY(1000 * millisecs);
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
