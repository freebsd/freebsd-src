/* $Id: clock.c,v 1.1 1998/06/10 10:52:13 dfr Exp $ */
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
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/cpuconf.h>
#include <machine/clockvar.h>

#define	SECMIN	((unsigned)60)			/* seconds per minute */
#define	SECHOUR	((unsigned)(60*SECMIN))		/* seconds per hour */
#define	SECDAY	((unsigned)(24*SECHOUR))	/* seconds per day */
#define	SECYR	((unsigned)(365*SECDAY))	/* seconds per common year */

#define	LEAPYEAR(year)	(((year) % 4) == 0)

device_t clockdev;
int clockinitted;
int tickfix;
int tickfixinterval;
int	wall_cmos_clock;	/* wall	CMOS clock assumed if != 0 */

extern int cycles_per_sec;

static timecounter_get_t	alpha_get_timecount;
static timecounter_pps_t	alpha_poll_pps;

static struct timecounter alpha_timecounter[3] = {
	alpha_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
 	~0u,			/* counter_mask */
	0,			/* frequency */
	"alpha"			/* name */
};

SYSCTL_OPAQUE(_debug, OID_AUTO, alpha_timecounter, CTLFLAG_RD, 
	alpha_timecounter, sizeof(alpha_timecounter), "S,timecounter", "");

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

static void handleclock(void* arg);

void
clockattach(device_t dev)
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
	if (clockdev == NULL)
		panic("cpu_initclocks: no clock attached");

	tick = 1000000 / hz;	/* number of microseconds between interrupts */
	tickfix = 1000000 - (hz * tick);
	if (tickfix) {
		int ftp;

		ftp = min(ffs(tickfix), ffs(hz));
		tickfix >>= (ftp - 1);
		tickfixinterval = hz >> (ftp - 1);
        }

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
	last_time = alpha_rpcc();
	scaled_ticks_per_cycle = ((u_int64_t)hz << FIX_SHIFT) / cycles_per_sec;
	max_cycles_per_tick = 2*cycles_per_sec / hz;

	alpha_timecounter[0].tc_frequency = cycles_per_sec;
	init_timecounter(alpha_timecounter);

	platform.clockintr = (void (*) __P((void *))) handleclock;

	/*
	 * Get the clock started.
	 */
	CLOCK_INIT(clockdev);
}

static void
handleclock(void* arg)
{
	u_int32_t now = alpha_rpcc();
	u_int32_t delta = now - last_time;
	last_time = now;

	if (delta > max_cycles_per_tick) {
		int i, missed_ticks;
		missed_ticks = (delta * scaled_ticks_per_cycle) >> FIX_SHIFT;
		for (i = 0; i < missed_ticks; i++)
			hardclock(arg);
	}
	hardclock(arg);
}

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
		printf("WARNING: preposterous time in file system");
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
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		s = splclock();
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		set_timecounter(&ts);
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
	ts.tv_nsec = 0;
	set_timecounter(&ts);
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
		printf("WARNING: clock %s %d days",
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

	s = splclock();
	tm = time_second;
	splx(s);

	if (!clockinitted)
		return;

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

static unsigned
alpha_get_timecount(struct timecounter* tc)
{
    return alpha_rpcc();
}

