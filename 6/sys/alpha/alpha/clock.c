/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 *	$NetBSD: clock.c,v 1.20 1998/01/31 10:32:47 ross Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_clock.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/clockvar.h>
#include <machine/cpuconf.h>
#include <machine/md_var.h>
#include <machine/rpb.h>	/* for CPU definitions, etc */
#include <machine/ppireg.h>
#include <machine/timerreg.h>

#include <isa/isareg.h>

#define	SECMIN	((unsigned)60)			/* seconds per minute */
#define	SECHOUR	((unsigned)(60*SECMIN))		/* seconds per hour */
#define	SECDAY	((unsigned)(24*SECHOUR))	/* seconds per day */
#define	SECYR	((unsigned)(365*SECDAY))	/* seconds per common year */

/*
 * According to OSF/1's /usr/sys/include/arch/alpha/clock.h,
 * the console adjusts the RTC years 13..19 to 93..99 and
 * 20..40 to 00..20. (historical reasons?)
 * DEC Unix uses an offset to the year to stay outside
 * the dangerous area for the next couple of years.
 */
#define UNIX_YEAR_OFFSET 52 /* 41=>1993, 12=>2064 */

static int clock_year_offset = 0;

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y)	(((y) % 4) == 0)

device_t clockdev;
int clockinitted;
int	adjkerntz;		/* local offset	from GMT in seconds */
int	disable_rtc_set;	/* disable resettodr() if != 0 */
int	wall_cmos_clock;	/* wall	CMOS clock assumed if != 0 */
struct mtx clock_lock;
static	int	beeping = 0;

#define	TIMER_DIV(x) ((timer_freq + (x) / 2) / (x))

#ifndef TIMER_FREQ
#define TIMER_FREQ   1193182
#endif
u_int32_t timer_freq = TIMER_FREQ;

extern int cycles_per_sec;

static timecounter_get_t	i8254_get_timecount;
static timecounter_get_t	alpha_get_timecount;

static struct timecounter alpha_timecounter = {
	alpha_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
 	~0u,			/* counter_mask */
	0,			/* frequency */
	"alpha",		/* name */
	800,			/* quality */
};

static struct timecounter i8254_timecounter = {
	i8254_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffff,			/* counter_mask */
	0,			/* frequency */
	"i8254"			/* name */
};

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

/* static	u_char	timer0_state; */
static	u_char	timer2_state;

static void calibrate_clocks(u_int32_t firmware_freq, u_int32_t *pcc,
    u_int32_t *timer);
static void set_timer_freq(u_int freq, int intr_freq);

void
clockattach(device_t dev)
{
	u_int32_t pcc, freq, delta;

	/*
	 * Just bookkeeping.
	 */
	if (clockdev)
		panic("clockattach: multiple clocks");
	clockdev = dev;

	calibrate_clocks(cycles_per_sec, &pcc, &freq);
	cycles_per_sec = pcc;

	/*
	 * XXX: TurboLaser doesn't have an i8254 counter.
	 * XXX: A replacement is needed, and another method
	 * XXX: of determining this would be nice.
	 */
	if (hwrpb->rpb_type == ST_DEC_21000) {
		goto out;
	}
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

out:
#ifdef EVCNT_COUNTERS
	evcnt_attach(dev, "intr", &clock_intr_evcnt);
#else
	/* nothing */ ;
#endif
}

/*
 * Machine-dependent clock routines.
 *
 * Startrtclock restarts the real-time clock, which provides
 * hardclock interrupts to kern_clock.c.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.  Its primary function is to use some file
 * system information in case the hardare clock lost state.
 *
 * Resettodr restores the time of day hardware after a time change.
 */

/*
 * Start the real-time and statistics clocks.
 */
void
cpu_initclocks()
{

	if (clockdev == NULL)
		panic("cpu_initclocks: no clock attached");

	tick = 1000000 / hz;	/* number of microseconds between interrupts */

	/*
	 * Establish the clock interrupt; it's a special case.
	 *
	 * We establish the clock interrupt this late because if
	 * we do it at clock attach time, we may have never been at
	 * spl0() since taking over the system.  Some versions of
	 * PALcode save a clock interrupt, which would get delivered
	 * when we spl0() in autoconf.c.  If established the clock
	 * interrupt handler earlier, that interrupt would go to
	 * hardclock, which would then fall over because p->p_stats
	 * isn't set at that time.
	 */

	/*
	 * XXX: TurboLaser doesn't have an i8254 counter.
	 * XXX: A replacement is needed, and another method
	 * XXX: of determining this would be nice.
	 */
	if (hwrpb->rpb_type != ST_DEC_21000)
		tc_init(&i8254_timecounter);

	if (mp_ncpus == 1) {
		alpha_timecounter.tc_frequency = cycles_per_sec;
		tc_init(&alpha_timecounter);
	}

	stathz = hz / 8;
	profhz = stathz;
	platform.clockintr = (void (*)(void *)) hardclock;

	/*
	 * Get the clock started.
	 */
	CLOCK_INIT(clockdev);
}

static __inline int get_8254_ctr(void);

static __inline int
get_8254_ctr(void)
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

static void
calibrate_clocks(u_int32_t firmware_freq, u_int32_t *pcc, u_int32_t *timer)
{
	u_int32_t start_pcc, stop_pcc;
	u_int count, prev_count, tot_count;
	int sec, start_sec;

	/*
	 * XXX: TurboLaser doesn't have an i8254 counter.
	 * XXX: A replacement is needed, and another method
	 * XXX: of determining this would be nice.
	 */
	if (hwrpb->rpb_type == ST_DEC_21000) {
		if (bootverbose)
			printf("Using firmware default frequency of %u Hz\n",
			    firmware_freq);
		*pcc = firmware_freq;
		*timer = 0;
		return;
	}
	if (bootverbose)
	        printf("Calibrating clock(s) ... ");

	set_timer_freq(timer_freq, hz);

	/* Read the mc146818A seconds counter. */
	if (CLOCK_GETSECS(clockdev, &sec))
		goto fail;

	/* Wait for the mC146818A seconds counter to change. */
	start_sec = sec;
	for (;;) {
		if (CLOCK_GETSECS(clockdev, &sec))
			goto fail;
		if (sec != start_sec)
			break;
	}

	/* Start keeping track of the PCC and i8254. */
	prev_count = get_8254_ctr();
	if (prev_count == 0)
		goto fail;
	tot_count = 0;

	start_pcc = alpha_rpcc();

	/*
	 * Wait for the mc146818A seconds counter to change.  Read the i8254
	 * counter for each iteration since this is convenient and only
	 * costs a few usec of inaccuracy. The timing of the final reads
	 * of the counters almost matches the timing of the initial reads,
	 * so the main cause of inaccuracy is the varying latency from 
	 * inside get_8254_ctr() or rtcin(RTC_STATUSA) to the beginning of the
	 * rtcin(RTC_SEC) that returns a changed seconds count.  The
	 * maximum inaccuracy from this cause is < 10 usec on 486's.
	 */
	start_sec = sec;
	for (;;) {
		if (CLOCK_GETSECS(clockdev, &sec))
			goto fail;
		count = get_8254_ctr();
		if (count == 0)
			goto fail;
		if (count > prev_count)
			tot_count += prev_count - (count - 0xffff);
		else
			tot_count += prev_count - count;
		prev_count = count;
		if (sec != start_sec)
			break;
	}

	/*
	 * Read the PCC again to work out frequency.
	 */
	stop_pcc = alpha_rpcc();

	if (bootverbose) {
	        printf("PCC clock: %u Hz (firmware %u Hz)\n",
		       stop_pcc - start_pcc, firmware_freq);
	        printf("i8254 clock: %u Hz\n", tot_count);
	}
	*pcc = stop_pcc - start_pcc;
	*timer = tot_count;
	return;

fail:
	if (bootverbose)
	        printf("failed, using firmware default of %u Hz\n",
		       firmware_freq);

	*pcc = firmware_freq;
	*timer = 0;
	return;
}

static void
set_timer_freq(u_int freq, int intr_freq)
{

	mtx_lock_spin(&clock_lock);
	timer_freq = freq;
	i8254_timecounter.tc_frequency = timer_freq;
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR0, 0);
	outb(TIMER_CNTR0, 0);
	mtx_unlock_spin(&clock_lock);
}

void
cpu_startprofclock(void)
{

	/* nothing to do */
}

void
cpu_stopprofclock(void)
{

	/* nothing to do */
}

/*
 * This code is defunct after 2099.
 * Will Unix still be here then??
 */
static short dayyr[12] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/*
 * Initialze the time of day register, based on the time base which is, e.g.
 * from a filesystem.  Base provides the time to within six months,
 * and the time of year clock (if any) provides the rest.
 */
void
inittodr(time_t base)
{
	struct clocktime ct;
	struct timespec ts;
	int clock_compat_osf1, todr_unreliable;
	int days, yr;

	if (getenv_int("clock_compat_osf1", &clock_compat_osf1)) {
		if (clock_compat_osf1)
			clock_year_offset = UNIX_YEAR_OFFSET;
	}

	todr_unreliable = 0;
	CLOCK_GET(clockdev, base, &ct);

#ifdef DEBUG
	printf("readclock: %d/%d/%d/%d/%d/%d\n", ct.year, ct.mon, ct.day,
		ct.hour, ct.min, ct.sec);
#endif
	ct.year += clock_year_offset;
	if (ct.year < 70)
		ct.year += 100;

	/* simple sanity checks */
	if (ct.year < 70 || ct.mon < 1 || ct.mon > 12 || ct.day < 1 ||
	    ct.day > 31 || ct.hour > 23 || ct.min > 59 || ct.sec > 59) {
		/*
		 * Believe the time in the filesystem for lack of
		 * anything better, resetting the TODR.
		 */
		ts.tv_sec = base;
		printf("WARNING: preposterous real-time clock");
		todr_unreliable = 1;
	} else {
		days = 0;
		for (yr = 70; yr < ct.year; yr++)
			days += LEAPYEAR(yr) ? 366 : 365;
		days += dayyr[ct.mon - 1] + ct.day - 1;
		if (LEAPYEAR(yr) && ct.mon > 2)
			days++;
		/* now have days since Jan 1, 1970; the rest is easy... */
		ts.tv_sec = days * SECDAY + ct.hour * SECHOUR +
		    ct.min * SECMIN + ct.sec;
		if (wall_cmos_clock)
			ts.tv_sec += adjkerntz;
		/*
		 * The time base comes from a saved time, whereas the real-
		 * time clock is supposed to represent the current time.
		 * It is logically not possible for a saved time to be
		 * larger than the current time, so if that happens, assume
		 * the real-time clock is off. Warn when the real-time
		 * clock is off by two or more days.
		 */
		if (ts.tv_sec < base) {
			ts.tv_sec = base;
			days = (base - ts.tv_sec) / (60L * 60L * 24L);
			if (days >= 2) {
				printf("WARNING: real-time clock lost %d days",
				    days);
				todr_unreliable = 1;
			}
		} 
	}
	ts.tv_nsec = 0;
	tc_setclock(&ts);
	clockinitted = 1;

	if (todr_unreliable) {
		printf(" -- CHECK AND RESET THE DATE!\n");
		resettodr();
	}
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECYEAR+2*SECDAY) (e.g. on Jan 2 just after midnight)
 * to wrap the TODR around.
 */
void
resettodr()
{
	register int t, t2, s;
	struct clocktime ct;
	unsigned long	tm;

	if (disable_rtc_set)
		return;

	s = splclock();
	tm = time_second;
	splx(s);

	if (!clockinitted)
		return;

	/* Calculate local time	to put in RTC */
	tm -= (wall_cmos_clock ? adjkerntz : 0);

	/* compute the day of week. */
	t2 = tm / SECDAY;
	ct.dow = (t2 + 4) % 7;	/* 1/1/1970 was thursday */

	/* compute the year */
	ct.year = 69;
	t = t2;			/* XXX ? */
	while (t2 >= 0) {	/* whittle off years */
		t = t2;
		ct.year++;
		t2 -= LEAPYEAR(ct.year) ? 366 : 365;
	}

	/* t = month + day; separate */
	t2 = LEAPYEAR(ct.year);
	for (ct.mon = 1; ct.mon < 12; ct.mon++)
		if (t < dayyr[ct.mon] + (t2 && ct.mon > 1))
			break;

	ct.day = t - dayyr[ct.mon - 1] + 1;
	if (t2 && ct.mon > 2)
		ct.day--;

	/* the rest is easy */
	t = tm % SECDAY;
	ct.hour = t / SECHOUR;
	t %= 3600;
	ct.min = t / SECMIN;
	ct.sec = t % SECMIN;

	ct.year = (ct.year - clock_year_offset) % 100;
	CLOCK_SET(clockdev, &ct);
}

static unsigned
i8254_get_timecount(struct timecounter *tc)
{

	return (0xffff - get_8254_ctr());
}

static unsigned
alpha_get_timecount(struct timecounter* tc)
{
	return alpha_rpcc();
}

int
acquire_timer2(int mode)
{
	/*
	 * XXX: TurboLaser doesn't have an i8254 counter.
	 * XXX: A replacement is needed, and another method
	 * XXX: of determining this would be nice.
	 */
	if (hwrpb->rpb_type == ST_DEC_21000) {
		return (0);
	}

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
release_timer2(void)
{
	/*
	 * XXX: TurboLaser doesn't have an i8254 counter.
	 * XXX: A replacement is needed, and another method
	 * XXX: of determining this would be nice.
	 */
	if (hwrpb->rpb_type == ST_DEC_21000) {
		return (0);
	}

	if (timer2_state != ACQUIRED)
		return (-1);
	timer2_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
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
	/*
	 * XXX: TurboLaser doesn't have an i8254 counter.
	 * XXX: A replacement is needed, and another method
	 * XXX: of determining this would be nice.
	 */
	if (hwrpb->rpb_type == ST_DEC_21000) {
		return (0);
	}

	mtx_lock_spin(&clock_lock);

	if (timer_spkr_acquire())
		if (!beeping) {
			/* Something else owns it. */
			mtx_unlock_spin(&clock_lock);
			return (-1); /* XXX Should be EBUSY, but nobody cares anyway. */
		}

	if (pitch) pitch = TIMER_DIV(pitch);

	spkr_set_pitch(pitch);
	mtx_unlock_spin(&clock_lock);
	if (!beeping) {
		/* enable counter2 output to speaker */
		if (pitch) ppi_spkr_on();
		beeping = period;
		timeout(sysbeepstop, (void *)NULL, period);
	}
	return (0);
}

