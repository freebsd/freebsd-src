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

/*
 * inittodr, settodr and support routines written
 * by Christoph Robitschko <chmr@edvz.tu-graz.ac.at>
 *
 * reintroduced and updated by Chris Stenton <chris@gnome.co.uk> 8/10/94
 */

#include "opt_clock.h"
#include "opt_isa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kdb.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/cons.h>
#include <sys/power.h>

#include <machine/clock.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#include <machine/apicvar.h>
#include <machine/specialreg.h>
#include <machine/ppireg.h>
#include <machine/timerreg.h>

#include <isa/rtc.h>
#ifdef DEV_ISA
#include <isa/isareg.h>
#include <isa/isavar.h>
#endif

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y) (((u_int)(y) % 4 == 0) ? 1 : 0)
#define DAYSPERYEAR   (31+28+31+30+31+30+31+31+30+31+30+31)

#define	TIMER_DIV(x) ((timer_freq + (x) / 2) / (x))

int	adjkerntz;		/* local offset from GMT in seconds */
int	clkintr_pending;
int	disable_rtc_set;	/* disable resettodr() if != 0 */
int	pscnt = 1;
int	psdiv = 1;
int	statclock_disable;
#ifndef TIMER_FREQ
#define TIMER_FREQ   1193182
#endif
u_int	timer_freq = TIMER_FREQ;
int	timer0_max_count;
int	wall_cmos_clock;	/* wall CMOS clock assumed if != 0 */
struct mtx clock_lock;
#define	RTC_LOCK	mtx_lock_spin(&clock_lock)
#define	RTC_UNLOCK	mtx_unlock_spin(&clock_lock)

static	int	beeping = 0;
static	const u_char daysinmonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
static	u_int	hardclock_max_count;
static	struct intsrc *i8254_intsrc;
static	u_int32_t i8254_lastcount;
static	u_int32_t i8254_offset;
static	int	(*i8254_pending)(struct intsrc *);
static	int	i8254_ticked;
static	int	using_lapic_timer;
static	u_char	rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;
static	u_char	rtc_statusb = RTCSB_24HR;

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

static	u_char	timer2_state;

static	unsigned i8254_get_timecount(struct timecounter *tc);
static	unsigned i8254_simple_get_timecount(struct timecounter *tc);
static	void	set_timer_freq(u_int freq, int intr_freq);

static struct timecounter i8254_timecounter = {
	i8254_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"i8254",		/* name */
	0			/* quality */
};

static void
clkintr(struct clockframe *frame)
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
	if (!using_lapic_timer)
		hardclock(frame);
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
release_timer2()
{

	if (timer2_state != ACQUIRED)
		return (-1);
	timer2_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
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
static void
rtcintr(struct clockframe *frame)
{

	while (rtcin(RTC_INTR) & RTCIR_PERIOD) {
		if (profprocs != 0) {
			if (--pscnt == 0)
				pscnt = psdiv;
			profclock(frame);
		}
		if (pscnt == psdiv)
			statclock(frame);
	}
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
#ifdef KDB
		if (kdb_active) {
			inb(0x84);
			tick = prev_tick - 1;
			if (tick <= 0)
				tick = timer0_max_count;
		} else
#endif
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
	ppi_spkr_off();		/* disable counter2 output to speaker */
	timer_spkr_release();
	beeping = 0;
}

int
sysbeep(int pitch, int period)
{
	int x = splclock();

	if (timer_spkr_acquire())
		if (!beeping) {
			/* Something else owns it. */
			splx(x);
			return (-1); /* XXX Should be EBUSY, but nobody cares anyway. */
		}
	mtx_lock_spin(&clock_lock);
	spkr_set_pitch(pitch);
	mtx_unlock_spin(&clock_lock);
	if (!beeping) {
		/* enable counter2 output to speaker */
		ppi_spkr_on();
		beeping = period;
		timeout(sysbeepstop, (void *)NULL, period);
	}
	splx(x);
	return (0);
}

/*
 * RTC support routines
 */

int
rtcin(reg)
	int reg;
{
	u_char val;

	RTC_LOCK;
	outb(IO_RTC, reg);
	inb(0x84);
	val = inb(IO_RTC + 1);
	inb(0x84);
	RTC_UNLOCK;
	return (val);
}

static __inline void
writertc(u_char reg, u_char val)
{

	RTC_LOCK;
	inb(0x84);
	outb(IO_RTC, reg);
	inb(0x84);
	outb(IO_RTC + 1, val);
	inb(0x84);		/* XXX work around wrong order in rtcin() */
	RTC_UNLOCK;
}

static __inline int
readrtc(int port)
{
	return(bcd2bin(rtcin(port)));
}

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

	i8254_timecounter.tc_frequency = freq;
	mtx_lock_spin(&clock_lock);
	timer_freq = freq;
	new_timer0_max_count = hardclock_max_count = TIMER_DIV(intr_freq);
	if (using_lapic_timer) {
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
		outb(TIMER_CNTR0, 0);
		outb(TIMER_CNTR0, 0);
	} else if (new_timer0_max_count != timer0_max_count) {
		timer0_max_count = new_timer0_max_count;
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
		outb(TIMER_CNTR0, timer0_max_count & 0xff);
		outb(TIMER_CNTR0, timer0_max_count >> 8);
	}
	mtx_unlock_spin(&clock_lock);
}

/*
 * Initialize 8254 timer 0 early so that it can be used in DELAY().
 * XXX initialization of other timers is unintentionally left blank.
 */
void
startrtclock()
{
	u_int delta, freq;

	writertc(RTC_STATUSA, rtc_statusa);
	writertc(RTC_STATUSB, RTCSB_24HR);

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
	tc_init(&i8254_timecounter);

	init_TSC();
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

	if (base) {
		s = splclock();
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
		splx(s);
	}

	/* Look if we have a RTC present and the time is valid */
	if (!(rtcin(RTC_STATUSD) & RTCSD_PWR))
		goto wrong_time;

	/* wait for time update to complete */
	/* If RTCSA_TUP is zero, we have at least 244us before next update */
	s = splhigh();
	while (rtcin(RTC_STATUSA) & RTCSA_TUP) {
		splx(s);
		s = splhigh();
	}

	days = 0;
#ifdef USE_RTC_CENTURY
	year = readrtc(RTC_YEAR) + readrtc(RTC_CENTURY) * 100;
#else
	year = readrtc(RTC_YEAR) + 1900;
	if (year < 1970)
		year += 100;
#endif
	if (year < 1970) {
		splx(s);
		goto wrong_time;
	}
	month = readrtc(RTC_MONTH);
	for (m = 1; m < month; m++)
		days += daysinmonth[m-1];
	if ((month > 2) && LEAPYEAR(year))
		days ++;
	days += readrtc(RTC_DAY) - 1;
	for (y = 1970; y < year; y++)
		days += DAYSPERYEAR + LEAPYEAR(y);
	sec = ((( days * 24 +
		  readrtc(RTC_HRS)) * 60 +
		  readrtc(RTC_MIN)) * 60 +
		  readrtc(RTC_SEC));
	/* sec now contains the number of seconds, since Jan 1 1970,
	   in the local time zone */

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

	if (disable_rtc_set)
		return;

	s = splclock();
	tm = time_second;
	splx(s);

	/* Disable RTC updates and interrupts. */
	writertc(RTC_STATUSB, RTCSB_HALT | RTCSB_24HR);

	/* Calculate local time to put in RTC */

	tm -= tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	writertc(RTC_SEC, bin2bcd(tm%60)); tm /= 60;	/* Write back Seconds */
	writertc(RTC_MIN, bin2bcd(tm%60)); tm /= 60;	/* Write back Minutes */
	writertc(RTC_HRS, bin2bcd(tm%24)); tm /= 24;	/* Write back Hours   */

	/* We have now the days since 01-01-1970 in tm */
	writertc(RTC_WDAY, (tm + 4) % 7 + 1);		/* Write back Weekday */
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
	rtcin(RTC_INTR);
}


/*
 * Start both clocks running.
 */
void
cpu_initclocks()
{
	int diag;

	using_lapic_timer = lapic_setup_clock();
	/*
	 * If we aren't using the local APIC timer to drive the kernel
	 * clocks, setup the interrupt handler for the 8254 timer 0 so
	 * that it can drive hardclock().  Otherwise, change the 8254
	 * timecounter to user a simpler algorithm.
	 */
	if (!using_lapic_timer || 1) {
		intr_add_handler("clk", 0, (driver_intr_t *)clkintr, NULL,
		    INTR_TYPE_CLK | INTR_FAST, NULL);
		i8254_intsrc = intr_lookup_source(0);
		if (i8254_intsrc != NULL)
			i8254_pending =
			    i8254_intsrc->is_pic->pic_source_pending;
	} else {
		i8254_timecounter.tc_get_timecount =
		    i8254_simple_get_timecount;
		i8254_timecounter.tc_counter_mask = 0xffff;
		set_timer_freq(timer_freq, hz);
	}

	/* Initialize RTC. */
	writertc(RTC_STATUSA, rtc_statusa);
	writertc(RTC_STATUSB, RTCSB_24HR);

	/*
	 * If the separate statistics clock hasn't been explicility disabled
	 * and we aren't already using the local APIC timer to drive the
	 * kernel clocks, then setup the RTC to periodically interrupt to
	 * drive statclock() and profclock().
	 */
	if (!statclock_disable && !using_lapic_timer) {
		diag = rtcin(RTC_DIAG);
		if (diag != 0)
			printf("RTC BIOS diagnostic error %b\n", diag, RTCDG_BITS);

	        /* Setting stathz to nonzero early helps avoid races. */
		stathz = RTC_NOPROFRATE;
		profhz = RTC_PROFRATE;

		/* Enable periodic interrupts from the RTC. */
		rtc_statusb |= RTCSB_PINTR;
		intr_add_handler("rtc", 8, (driver_intr_t *)rtcintr, NULL,
		    INTR_TYPE_CLK | INTR_FAST, NULL);

		writertc(RTC_STATUSB, rtc_statusb);
		rtcin(RTC_INTR);
	}

	init_TSC_tc();
}

void
cpu_startprofclock(void)
{

	if (using_lapic_timer)
		return;
	rtc_statusa = RTCSA_DIVIDER | RTCSA_PROF;
	writertc(RTC_STATUSA, rtc_statusa);
	psdiv = pscnt = psratio;
}

void
cpu_stopprofclock(void)
{

	if (using_lapic_timer)
		return;
	rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;
	writertc(RTC_STATUSA, rtc_statusa);
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
	freq = timer_freq;
	error = sysctl_handle_int(oidp, &freq, sizeof(freq), req);
	if (error == 0 && req->newptr != NULL)
		set_timer_freq(freq, hz);
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, i8254_freq, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_i8254_freq, "IU", "");

static unsigned
i8254_simple_get_timecount(struct timecounter *tc)
{
	u_int count;
	u_int high, low;

	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);
	count = 0xffff - ((high << 8) | low);
	mtx_unlock_spin(&clock_lock);
	return (count);
}

static unsigned
i8254_get_timecount(struct timecounter *tc)
{
	u_int count;
	u_int high, low;
	u_long rflags;

	rflags = read_rflags();
	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);
	count = timer0_max_count - ((high << 8) | low);
	if (count < i8254_lastcount ||
	    (!i8254_ticked && (clkintr_pending ||
	    ((count < 20 || (!(rflags & PSL_I) && count < timer0_max_count / 2u)) &&
	    i8254_pending != NULL && i8254_pending(i8254_intsrc))))) {
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
DRIVER_MODULE(attimer, acpi, attimer_driver, attimer_devclass, 0, 0);
#endif /* DEV_ISA */
