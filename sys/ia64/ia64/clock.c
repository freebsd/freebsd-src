/* $FreeBSD$ */
/* $NetBSD: clock.c,v 1.20 1998/01/31 10:32:47 ross Exp $ */

/*
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/timetc.h>

#include <machine/pal.h>
#include <machine/sal.h>
#include <machine/clock.h>
#include <machine/clockvar.h>
#include <isa/isareg.h>
#include <ia64/ia64/timerreg.h>

#define	SECMIN	((unsigned)60)			/* seconds per minute */
#define	SECHOUR	((unsigned)(60*SECMIN))		/* seconds per hour */
#define	SECDAY	((unsigned)(24*SECHOUR))	/* seconds per day */
#define	SECYR	((unsigned)(365*SECDAY))	/* seconds per common year */

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y)	(((y) % 4) == 0)

kobj_t clockdev;
int clockinitted;
int tickfix;
int tickfixinterval;
int	adjkerntz;		/* local offset	from GMT in seconds */
int	disable_rtc_set;	/* disable resettodr() if != 0 */
int	wall_cmos_clock;	/* wall	CMOS clock assumed if != 0 */
u_int64_t itm_reload;		/* reload ticks for clock */
static	int	beeping = 0;

#ifndef SMP
static timecounter_get_t	ia64_get_timecount;

static struct timecounter ia64_timecounter = {
	ia64_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
 	~0u,			/* counter_mask */
	0,			/* frequency */
	"IA64 ITC"		/* name */
};

#endif

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

/* static	u_char	timer0_state; */
static	u_char	timer2_state;

/*
 * Algorithm for missed clock ticks from Linux/alpha.
 */

/*
 * Shift amount by which scaled_ticks_per_cycle is scaled.  Shifting
 * by 48 gives us 16 bits for HZ while keeping the accuracy good even
 * for large CPU clock rates.
 */
#define FIX_SHIFT	48

static u_int64_t scaled_ticks_per_cycle;
static u_int32_t max_cycles_per_tick;
static u_int32_t last_time;

#if 0	/* not used yet */
static u_int32_t calibrate_clocks(u_int32_t firmware_freq);
#endif

void
clockattach(kobj_t dev)
{

	/*
	 * Just bookkeeping.
	 */
	if (clockdev)
		panic("clockattach: multiple clocks");
	clockdev = dev;
#ifdef EVCNT_COUNTERS
	evcnt_attach(dev, "intr", &clock_intr_evcnt);
#endif

	/*
	 * Get the clock started.
	 */
	CLOCK_INIT(clockdev);
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
 * Start the real-time and statistics clocks. Leave stathz 0 since there
 * are no other timers available.
 */
void
cpu_initclocks()
{
	u_int32_t freq;

	/*
	 * We use cr.itc and cr.itm to implement a 1024hz clock.
	 */
	hz = 1024;
	tick = 1000000 / hz;	/* number of microseconds between interrupts */
	tickfix = 1000000 - (hz * tick);
	if (tickfix) {
		int ftp;

		ftp = min(ffs(tickfix), ffs(hz));
		tickfix >>= (ftp - 1);
		tickfixinterval = hz >> (ftp - 1);
        }

	if (!itc_frequency)
		panic("Unknown clock frequency");

	freq = itc_frequency;
	last_time = ia64_get_itc();
	scaled_ticks_per_cycle = ((u_int64_t)hz << FIX_SHIFT) / freq;
	max_cycles_per_tick = 2*freq / hz;

#ifndef SMP
	ia64_timecounter.tc_frequency = freq;
	tc_init(&ia64_timecounter);
#endif

	itm_reload = (itc_frequency + hz/2) / hz;
	ia64_set_itm(ia64_get_itc() + itm_reload);
	ia64_set_itv(255);	/* highest priority class */

	stathz = 128;
}

#if 0	/* not used yet */
static u_int32_t
calibrate_clocks(u_int32_t firmware_freq)
{
	u_int32_t start_pcc, stop_pcc;
	int sec, start_sec;

	if (bootverbose)
	        printf("Calibrating clock(s) ... ");

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

	/* Start keeping track of the PCC. */
	start_pcc = ia64_get_itc();

	/*
	 * Wait for the mc146818A seconds counter to change.
	 */
	start_sec = sec;
	for (;;) {
		if (CLOCK_GETSECS(clockdev, &sec))
			goto fail;
		if (sec != start_sec)
			break;
	}

	/*
	 * Read the PCC again to work out frequency.
	 */
	stop_pcc = ia64_get_itc();

	if (bootverbose) {
	        printf("PCC clock: %u Hz (firmware %u Hz)\n",
		       stop_pcc - start_pcc, firmware_freq);
	}
	return (stop_pcc - start_pcc);

fail:
	if (bootverbose)
	        printf("failed, using firmware default of %u Hz\n",
		       firmware_freq);
	return (firmware_freq);
}
#endif

/*
 * We assume newhz is either stathz or profhz, and that neither will
 * change after being set up above.  Could recalculate intervals here
 * but that would be a drag.
 */
void
setstatclockrate(newhz)
	int newhz;
{

	/* nothing we can do */
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
inittodr(base)
	time_t base;
{
	register int days, yr;
	struct clocktime ct;
	time_t deltat;
	int badbase;
	int s;
	struct timespec ts;

	if (base < 5*SECYR) {
		printf("WARNING: preposterous time in filesystem");
		/* read the system clock anyway */
		base = 6*SECYR + 186*SECDAY + SECDAY/2;
		badbase = 1;
	} else
		badbase = 0;

	CLOCK_GET(clockdev, base, &ct);
	clockinitted = 1;

	/* simple sanity checks */
	if (ct.year < 70 || ct.mon < 1 || ct.mon > 12 || ct.day < 1 ||
	    ct.day > 31 || ct.hour > 23 || ct.min > 59 || ct.sec > 59) {
		/*
		 * Believe the time in the filesystem for lack of
		 * anything better, resetting the TODR.
		 */
		s = splclock();
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
		splx(s);
		if (!badbase) {
			printf("WARNING: preposterous clock chip time\n");
			resettodr();
		}
		goto bad;
	}
	days = 0;
	for (yr = 70; yr < ct.year; yr++)
		days += LEAPYEAR(yr) ? 366 : 365;
	days += dayyr[ct.mon - 1] + ct.day - 1;
	if (LEAPYEAR(yr) && ct.mon > 2)
		days++;
	/* now have days since Jan 1, 1970; the rest is easy... */
	s = splclock();
	ts.tv_sec = 
	    days * SECDAY + ct.hour * SECHOUR + ct.min * SECMIN + ct.sec;
	if (wall_cmos_clock)
	    ts.tv_sec += adjkerntz;
	ts.tv_nsec = 0;
	tc_setclock(&ts);
	splx(s);

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days;
		 * if so, assume something is amiss.
		 */
		deltat = ts.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;
		printf("WARNING: clock %s %ld days",
		    ts.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
	}
bad:
	printf(" -- CHECK AND RESET THE DATE!\n");
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

	CLOCK_SET(clockdev, &ct);
}

#ifndef SMP
static unsigned
ia64_get_timecount(struct timecounter* tc)
{
    return ia64_get_itc();
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
release_timer2()
{

	if (timer2_state != ACQUIRED)
		return (-1);
	timer2_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
}

static void
sysbeepstop(void *chan)
{
	outb(IO_PPI, inb(IO_PPI)&0xFC);	/* disable counter2 output to speaker */
	release_timer2();
	beeping = 0;
}

/*
 * Frequency of all three count-down timers; (TIMER_FREQ/freq) is the
 * appropriate count to generate a frequency of freq hz.
 */
#ifndef TIMER_FREQ
#define	TIMER_FREQ	1193182
#endif
#define TIMER_DIV(x) ((TIMER_FREQ+(x)/2)/(x))

int
sysbeep(int pitch, int period)
{
	int x = splhigh();

	if (acquire_timer2(TIMER_SQWAVE|TIMER_16BIT))
		if (!beeping) {
			/* Something else owns it. */
			splx(x);
			return (-1); /* XXX Should be EBUSY, but nobody cares anyway. */
		}

	if (pitch) pitch = TIMER_DIV(pitch);

	outb(TIMER_CNTR2, pitch);
	outb(TIMER_CNTR2, (pitch>>8));
	if (!beeping) {
		/* enable counter2 output to speaker */
		if (pitch) outb(IO_PPI, inb(IO_PPI) | 3);
		beeping = period;
		timeout(sysbeepstop, (void *)NULL, period);
	}
	splx(x);
	return (0);
}

