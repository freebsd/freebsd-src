/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 * $Id: kern_clock.c,v 1.18 1995/11/08 08:45:58 phk Exp $
 */

/* Portions of this software are covered by the following: */
/******************************************************************************
 *                                                                            *
 * Copyright (c) David L. Mills 1993, 1994                                    *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appears in all copies and that both the    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name University of Delaware not be used in     *
 * advertising or publicity pertaining to distribution of the software        *
 * without specific, written prior permission.  The University of Delaware    *
 * makes no representations about the suitability this software for any       *
 * purpose.  It is provided "as is" without express or implied warranty.      *
 *                                                                            *
 *****************************************************************************/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dkstat.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/timex.h>
#include <vm/vm.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/clock.h>

#ifdef GPROF
#include <sys/gmon.h>
#endif

/*
 * System initialization
 */

static void initclocks __P((void *udata));
SYSINIT(clocks, SI_SUB_CLOCKS, SI_ORDER_FIRST, initclocks, NULL)


/* Does anybody else really care about these? */
struct callout *callfree, *callout, calltodo;

/* Some of these don't belong here, but it's easiest to concentrate them. */
long cp_time[CPUSTATES];
long dk_seek[DK_NDRIVE];
long dk_time[DK_NDRIVE];
long dk_wds[DK_NDRIVE];
long dk_wpms[DK_NDRIVE];
long dk_xfer[DK_NDRIVE];

int dk_busy;
int dk_ndrive = 0;
char dk_names[DK_NDRIVE][DK_NAMELEN];

long tk_cancc;
long tk_nin;
long tk_nout;
long tk_rawcc;

/*
 * Clock handling routines.
 *
 * This code is written to operate with two timers that run independently of
 * each other.  The main clock, running hz times per second, is used to keep
 * track of real time.  The second timer handles kernel and user profiling,
 * and does resource use estimation.  If the second timer is programmable,
 * it is randomized to avoid aliasing between the two clocks.  For example,
 * the randomization prevents an adversary from always giving up the cpu
 * just before its quantum expires.  Otherwise, it would never accumulate
 * cpu ticks.  The mean frequency of the second timer is stathz.
 *
 * If no second timer exists, stathz will be zero; in this case we drive
 * profiling and statistics off the main clock.  This WILL NOT be accurate;
 * do not do it unless absolutely necessary.
 *
 * The statistics clock may (or may not) be run at a higher rate while
 * profiling.  This profile clock runs at profhz.  We require that profhz
 * be an integral multiple of stathz.
 *
 * If the statistics clock is running fast, it must be divided by the ratio
 * profhz/stathz for statistics.  (For profiling, every tick counts.)
 */

/*
 * TODO:
 *	allocate more timeout table slots when table overflows.
 */

/*
 * Bump a timeval by a small number of usec's.
 */
#define BUMPTIME(t, usec) { \
	register volatile struct timeval *tp = (t); \
	register long us; \
 \
	tp->tv_usec = us = tp->tv_usec + (usec); \
	if (us >= 1000000) { \
		tp->tv_usec = us - 1000000; \
		tp->tv_sec++; \
	} \
}

int	stathz;
int	profhz;
int	profprocs;
int	ticks;
static int psdiv, pscnt;	/* prof => stat divider */
int	psratio;		/* ratio: prof / stat */

volatile struct	timeval time;
volatile struct	timeval mono_time;

/*
 * Phase-lock loop (PLL) definitions
 *
 * The following variables are read and set by the ntp_adjtime() system
 * call.
 *
 * time_state shows the state of the system clock, with values defined
 * in the timex.h header file.
 *
 * time_status shows the status of the system clock, with bits defined
 * in the timex.h header file.
 *
 * time_offset is used by the PLL to adjust the system time in small
 * increments.
 *
 * time_constant determines the bandwidth or "stiffness" of the PLL.
 *
 * time_tolerance determines maximum frequency error or tolerance of the
 * CPU clock oscillator and is a property of the architecture; however,
 * in principle it could change as result of the presence of external
 * discipline signals, for instance.
 *
 * time_precision is usually equal to the kernel tick variable; however,
 * in cases where a precision clock counter or external clock is
 * available, the resolution can be much less than this and depend on
 * whether the external clock is working or not.
 *
 * time_maxerror is initialized by a ntp_adjtime() call and increased by
 * the kernel once each second to reflect the maximum error
 * bound growth.
 *
 * time_esterror is set and read by the ntp_adjtime() call, but
 * otherwise not used by the kernel.
 */
int time_status = STA_UNSYNC;	/* clock status bits */
int time_state = TIME_OK;	/* clock state */
long time_offset = 0;		/* time offset (us) */
long time_constant = 0;		/* pll time constant */
long time_tolerance = MAXFREQ;	/* frequency tolerance (scaled ppm) */
long time_precision = 1;	/* clock precision (us) */
long time_maxerror = MAXPHASE;	/* maximum error (us) */
long time_esterror = MAXPHASE;	/* estimated error (us) */

/*
 * The following variables establish the state of the PLL and the
 * residual time and frequency offset of the local clock. The scale
 * factors are defined in the timex.h header file.
 *
 * time_phase and time_freq are the phase increment and the frequency
 * increment, respectively, of the kernel time variable at each tick of
 * the clock.
 *
 * time_freq is set via ntp_adjtime() from a value stored in a file when
 * the synchronization daemon is first started. Its value is retrieved
 * via ntp_adjtime() and written to the file about once per hour by the
 * daemon.
 *
 * time_adj is the adjustment added to the value of tick at each timer
 * interrupt and is recomputed at each timer interrupt.
 *
 * time_reftime is the second's portion of the system time on the last
 * call to ntp_adjtime(). It is used to adjust the time_freq variable
 * and to increase the time_maxerror as the time since last update
 * increases.
 */
long time_phase = 0;		/* phase offset (scaled us) */
long time_freq = 0;		/* frequency offset (scaled ppm) */
long time_adj = 0;		/* tick adjust (scaled 1 / hz) */
long time_reftime = 0;		/* time at last adjustment (s) */

#ifdef PPS_SYNC
/*
 * The following variables are used only if the if the kernel PPS
 * discipline code is configured (PPS_SYNC). The scale factors are
 * defined in the timex.h header file.
 *
 * pps_time contains the time at each calibration interval, as read by
 * microtime().
 *
 * pps_offset is the time offset produced by the time median filter
 * pps_tf[], while pps_jitter is the dispersion measured by this
 * filter.
 *
 * pps_freq is the frequency offset produced by the frequency median
 * filter pps_ff[], while pps_stabil is the dispersion measured by
 * this filter.
 *
 * pps_usec is latched from a high resolution counter or external clock
 * at pps_time. Here we want the hardware counter contents only, not the
 * contents plus the time_tv.usec as usual.
 *
 * pps_valid counts the number of seconds since the last PPS update. It
 * is used as a watchdog timer to disable the PPS discipline should the
 * PPS signal be lost.
 *
 * pps_glitch counts the number of seconds since the beginning of an
 * offset burst more than tick/2 from current nominal offset. It is used
 * mainly to suppress error bursts due to priority conflicts between the
 * PPS interrupt and timer interrupt.
 *
 * pps_count counts the seconds of the calibration interval, the
 * duration of which is pps_shift in powers of two.
 *
 * pps_intcnt counts the calibration intervals for use in the interval-
 * adaptation algorithm. It's just too complicated for words.
 */
struct timeval pps_time;	/* kernel time at last interval */
long pps_offset = 0;		/* pps time offset (us) */
long pps_jitter = MAXTIME;	/* pps time dispersion (jitter) (us) */
long pps_tf[] = {0, 0, 0};	/* pps time offset median filter (us) */
long pps_freq = 0;		/* frequency offset (scaled ppm) */
long pps_stabil = MAXFREQ;	/* frequency dispersion (scaled ppm) */
long pps_ff[] = {0, 0, 0};	/* frequency offset median filter */
long pps_usec = 0;		/* microsec counter at last interval */
long pps_valid = PPS_VALID;	/* pps signal watchdog counter */
int pps_glitch = 0;		/* pps signal glitch counter */
int pps_count = 0;		/* calibration interval counter (s) */
int pps_shift = PPS_SHIFT;	/* interval duration (s) (shift) */
int pps_intcnt = 0;		/* intervals at current duration */

/*
 * PPS signal quality monitors
 *
 * pps_jitcnt counts the seconds that have been discarded because the
 * jitter measured by the time median filter exceeds the limit MAXTIME
 * (100 us).
 *
 * pps_calcnt counts the frequency calibration intervals, which are
 * variable from 4 s to 256 s.
 *
 * pps_errcnt counts the calibration intervals which have been discarded
 * because the wander exceeds the limit MAXFREQ (100 ppm) or where the
 * calibration interval jitter exceeds two ticks.
 *
 * pps_stbcnt counts the calibration intervals that have been discarded
 * because the frequency wander exceeds the limit MAXFREQ / 4 (25 us).
 */
long pps_jitcnt = 0;		/* jitter limit exceeded */
long pps_calcnt = 0;		/* calibration intervals */
long pps_errcnt = 0;		/* calibration errors */
long pps_stbcnt = 0;		/* stability limit exceeded */
#endif /* PPS_SYNC */

/* XXX none of this stuff works under FreeBSD */
#ifdef EXT_CLOCK
/*
 * External clock definitions
 *
 * The following definitions and declarations are used only if an
 * external clock (HIGHBALL or TPRO) is configured on the system.
 */
#define CLOCK_INTERVAL 30	/* CPU clock update interval (s) */

/*
 * The clock_count variable is set to CLOCK_INTERVAL at each PPS
 * interrupt and decremented once each second.
 */
int clock_count = 0;		/* CPU clock counter */

#ifdef HIGHBALL
/*
 * The clock_offset and clock_cpu variables are used by the HIGHBALL
 * interface. The clock_offset variable defines the offset between
 * system time and the HIGBALL counters. The clock_cpu variable contains
 * the offset between the system clock and the HIGHBALL clock for use in
 * disciplining the kernel time variable.
 */
extern struct timeval clock_offset; /* Highball clock offset */
long clock_cpu = 0;		/* CPU clock adjust */
#endif /* HIGHBALL */
#endif /* EXT_CLOCK */

/*
 * hardupdate() - local clock update
 *
 * This routine is called by ntp_adjtime() to update the local clock
 * phase and frequency. This is used to implement an adaptive-parameter,
 * first-order, type-II phase-lock loop. The code computes new time and
 * frequency offsets each time it is called. The hardclock() routine
 * amortizes these offsets at each tick interrupt. If the kernel PPS
 * discipline code is configured (PPS_SYNC), the PPS signal itself
 * determines the new time offset, instead of the calling argument.
 * Presumably, calls to ntp_adjtime() occur only when the caller
 * believes the local clock is valid within some bound (+-128 ms with
 * NTP). If the caller's time is far different than the PPS time, an
 * argument will ensue, and it's not clear who will lose.
 *
 * For default SHIFT_UPDATE = 12, the offset is limited to +-512 ms, the
 * maximum interval between updates is 4096 s and the maximum frequency
 * offset is +-31.25 ms/s.
 *
 * Note: splclock() is in effect.
 */
void
hardupdate(offset)
	long offset;
{
	long ltemp, mtemp;

	if (!(time_status & STA_PLL) && !(time_status & STA_PPSTIME))
		return;
	ltemp = offset;
#ifdef PPS_SYNC
	if (time_status & STA_PPSTIME && time_status & STA_PPSSIGNAL)
		ltemp = pps_offset;
#endif /* PPS_SYNC */
	if (ltemp > MAXPHASE)
		time_offset = MAXPHASE << SHIFT_UPDATE;
	else if (ltemp < -MAXPHASE)
		time_offset = -(MAXPHASE << SHIFT_UPDATE);
	else
		time_offset = ltemp << SHIFT_UPDATE;
	mtemp = time.tv_sec - time_reftime;
	time_reftime = time.tv_sec;
	if (mtemp > MAXSEC)
		mtemp = 0;

	/* ugly multiply should be replaced */
	if (ltemp < 0)
		time_freq -= (-ltemp * mtemp) >> (time_constant +
		    time_constant + SHIFT_KF - SHIFT_USEC);
	else
		time_freq += (ltemp * mtemp) >> (time_constant +
		    time_constant + SHIFT_KF - SHIFT_USEC);
	if (time_freq > time_tolerance)
		time_freq = time_tolerance;
	else if (time_freq < -time_tolerance)
		time_freq = -time_tolerance;
}



/*
 * Initialize clock frequencies and start both clocks running.
 */
/* ARGSUSED*/
static void
initclocks(udata)
	void *udata;		/* not used*/
{
	register int i;

	/*
	 * Set divisors to 1 (normal case) and let the machine-specific
	 * code do its bit.
	 */
	psdiv = pscnt = 1;
	cpu_initclocks();

	/*
	 * Compute profhz/stathz, and fix profhz if needed.
	 */
	i = stathz ? stathz : hz;
	if (profhz == 0)
		profhz = i;
	psratio = profhz / i;
}

/*
 * The real-time timer, interrupting hz times per second.
 */
void
hardclock(frame)
	register struct clockframe *frame;
{
	register struct callout *p1;
	register struct proc *p;
	register int needsoft;

	/*
	 * Update real-time timeout queue.
	 * At front of queue are some number of events which are ``due''.
	 * The time to these is <= 0 and if negative represents the
	 * number of ticks which have passed since it was supposed to happen.
	 * The rest of the q elements (times > 0) are events yet to happen,
	 * where the time for each is given as a delta from the previous.
	 * Decrementing just the first of these serves to decrement the time
	 * to all events.
	 */
	needsoft = 0;
	for (p1 = calltodo.c_next; p1 != NULL; p1 = p1->c_next) {
		if (--p1->c_time > 0)
			break;
		needsoft = 1;
		if (p1->c_time == 0)
			break;
	}

	p = curproc;
	if (p) {
		register struct pstats *pstats;

		/*
		 * Run current process's virtual and profile time, as needed.
		 */
		pstats = p->p_stats;
		if (CLKF_USERMODE(frame) &&
		    timerisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0)
			psignal(p, SIGVTALRM);
		if (timerisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0)
			psignal(p, SIGPROF);
	}

	/*
	 * If no separate statistics clock is available, run it from here.
	 */
	if (stathz == 0)
		statclock(frame);

	/*
	 * Increment the time-of-day.
	 */
	ticks++;
	{
		int time_update;
		struct timeval newtime = time;
		long ltemp;

		if (timedelta == 0) {
			time_update = CPU_THISTICKLEN(tick);
		} else {
			time_update = CPU_THISTICKLEN(tick) + tickdelta;
			timedelta -= tickdelta;
		}
		BUMPTIME(&mono_time, time_update);

		/*
		 * Compute the phase adjustment. If the low-order bits
		 * (time_phase) of the update overflow, bump the high-order bits
		 * (time_update).
		 */
		time_phase += time_adj;
		if (time_phase <= -FINEUSEC) {
		  ltemp = -time_phase >> SHIFT_SCALE;
		  time_phase += ltemp << SHIFT_SCALE;
		  time_update -= ltemp;
		}
		else if (time_phase >= FINEUSEC) {
		  ltemp = time_phase >> SHIFT_SCALE;
		  time_phase -= ltemp << SHIFT_SCALE;
		  time_update += ltemp;
		}

		newtime.tv_usec += time_update;
		/*
		 * On rollover of the second the phase adjustment to be used for
		 * the next second is calculated. Also, the maximum error is
		 * increased by the tolerance. If the PPS frequency discipline
		 * code is present, the phase is increased to compensate for the
		 * CPU clock oscillator frequency error.
		 *
		 * With SHIFT_SCALE = 23, the maximum frequency adjustment is
		 * +-256 us per tick, or 25.6 ms/s at a clock frequency of 100
		 * Hz. The time contribution is shifted right a minimum of two
		 * bits, while the frequency contribution is a right shift.
		 * Thus, overflow is prevented if the frequency contribution is
		 * limited to half the maximum or 15.625 ms/s.
		 */
		if (newtime.tv_usec >= 1000000) {
		  newtime.tv_usec -= 1000000;
		  newtime.tv_sec++;
		  time_maxerror += time_tolerance >> SHIFT_USEC;
		  if (time_offset < 0) {
		    ltemp = -time_offset >>
		      (SHIFT_KG + time_constant);
		    time_offset += ltemp;
		    time_adj = -ltemp <<
		      (SHIFT_SCALE - SHIFT_HZ - SHIFT_UPDATE);
		  } else {
		    ltemp = time_offset >>
		      (SHIFT_KG + time_constant);
		    time_offset -= ltemp;
		    time_adj = ltemp <<
		      (SHIFT_SCALE - SHIFT_HZ - SHIFT_UPDATE);
		  }
#ifdef PPS_SYNC
		  /*
		   * Gnaw on the watchdog counter and update the frequency
		   * computed by the pll and the PPS signal.
		   */
		  pps_valid++;
		  if (pps_valid == PPS_VALID) {
		    pps_jitter = MAXTIME;
		    pps_stabil = MAXFREQ;
		    time_status &= ~(STA_PPSSIGNAL | STA_PPSJITTER |
				     STA_PPSWANDER | STA_PPSERROR);
		  }
		  ltemp = time_freq + pps_freq;
#else
		  ltemp = time_freq;
#endif /* PPS_SYNC */
		  if (ltemp < 0)
		    time_adj -= -ltemp >>
		      (SHIFT_USEC + SHIFT_HZ - SHIFT_SCALE);
		  else
		    time_adj += ltemp >>
		      (SHIFT_USEC + SHIFT_HZ - SHIFT_SCALE);

		  /*
		   * When the CPU clock oscillator frequency is not a
		   * power of two in Hz, the SHIFT_HZ is only an
		   * approximate scale factor. In the SunOS kernel, this
		   * results in a PLL gain factor of 1/1.28 = 0.78 what it
		   * should be. In the following code the overall gain is
		   * increased by a factor of 1.25, which results in a
		   * residual error less than 3 percent.
		   */
		  /* Same thing applies for FreeBSD --GAW */
		  if (hz == 100) {
		    if (time_adj < 0)
		      time_adj -= -time_adj >> 2;
		    else
		      time_adj += time_adj >> 2;
		  }

		  /* XXX - this is really bogus, but can't be fixed until
		     xntpd's idea of the system clock is fixed to know how
		     the user wants leap seconds handled; in the mean time,
		     we assume that users of NTP are running without proper
		     leap second support (this is now the default anyway) */
		  /*
		   * Leap second processing. If in leap-insert state at
		   * the end of the day, the system clock is set back one
		   * second; if in leap-delete state, the system clock is
		   * set ahead one second. The microtime() routine or
		   * external clock driver will insure that reported time
		   * is always monotonic. The ugly divides should be
		   * replaced.
		   */
		  switch (time_state) {

		  case TIME_OK:
		    if (time_status & STA_INS)
		      time_state = TIME_INS;
		    else if (time_status & STA_DEL)
		      time_state = TIME_DEL;
		    break;

		  case TIME_INS:
		    if (newtime.tv_sec % 86400 == 0) {
		      newtime.tv_sec--;
		      time_state = TIME_OOP;
		    }
		    break;

		  case TIME_DEL:
		    if ((newtime.tv_sec + 1) % 86400 == 0) {
		      newtime.tv_sec++;
		      time_state = TIME_WAIT;
		    }
		    break;

		  case TIME_OOP:
		    time_state = TIME_WAIT;
		    break;

		  case TIME_WAIT:
		    if (!(time_status & (STA_INS | STA_DEL)))
		      time_state = TIME_OK;
		  }
		}
		CPU_CLOCKUPDATE(&time, &newtime);
	}

	/*
	 * Process callouts at a very low cpu priority, so we don't keep the
	 * relatively high clock interrupt priority any longer than necessary.
	 */
	if (needsoft) {
		if (CLKF_BASEPRI(frame)) {
			/*
			 * Save the overhead of a software interrupt;
			 * it will happen as soon as we return, so do it now.
			 */
			(void)splsoftclock();
			softclock();
		} else
			setsoftclock();
	}
}

/*
 * Software (low priority) clock interrupt.
 * Run periodic events from timeout queue.
 */
/*ARGSUSED*/
void
softclock()
{
	register struct callout *c;
	register void *arg;
	register void (*func) __P((void *));
	register int s;

	s = splhigh();
	while ((c = calltodo.c_next) != NULL && c->c_time <= 0) {
		func = c->c_func;
		arg = c->c_arg;
		calltodo.c_next = c->c_next;
		c->c_next = callfree;
		callfree = c;
		splx(s);
		(*func)(arg);
		(void) splhigh();
	}
	splx(s);
}

/*
 * timeout --
 *	Execute a function after a specified length of time.
 *
 * untimeout --
 *	Cancel previous timeout function call.
 *
 *	See AT&T BCI Driver Reference Manual for specification.  This
 *	implementation differs from that one in that no identification
 *	value is returned from timeout, rather, the original arguments
 *	to timeout are used to identify entries for untimeout.
 */
void
timeout(ftn, arg, ticks)
	timeout_t ftn;
	void *arg;
	register int ticks;
{
	register struct callout *new, *p, *t;
	register int s;

	if (ticks <= 0)
		ticks = 1;

	/* Lock out the clock. */
	s = splhigh();

	/* Fill in the next free callout structure. */
	if (callfree == NULL)
		panic("timeout table full");
	new = callfree;
	callfree = new->c_next;
	new->c_arg = arg;
	new->c_func = ftn;

	/*
	 * The time for each event is stored as a difference from the time
	 * of the previous event on the queue.  Walk the queue, correcting
	 * the ticks argument for queue entries passed.  Correct the ticks
	 * value for the queue entry immediately after the insertion point
	 * as well.  Watch out for negative c_time values; these represent
	 * overdue events.
	 */
	for (p = &calltodo;
	    (t = p->c_next) != NULL && ticks > t->c_time; p = t)
		if (t->c_time > 0)
			ticks -= t->c_time;
	new->c_time = ticks;
	if (t != NULL)
		t->c_time -= ticks;

	/* Insert the new entry into the queue. */
	p->c_next = new;
	new->c_next = t;
	splx(s);
}

void
untimeout(ftn, arg)
	timeout_t ftn;
	void *arg;
{
	register struct callout *p, *t;
	register int s;

	s = splhigh();
	for (p = &calltodo; (t = p->c_next) != NULL; p = t)
		if (t->c_func == ftn && t->c_arg == arg) {
			/* Increment next entry's tick count. */
			if (t->c_next && t->c_time > 0)
				t->c_next->c_time += t->c_time;

			/* Move entry from callout queue to callfree queue. */
			p->c_next = t->c_next;
			t->c_next = callfree;
			callfree = t;
			break;
		}
	splx(s);
}

/*
 * Compute number of hz until specified time.  Used to
 * compute third argument to timeout() from an absolute time.
 */
int
hzto(tv)
	struct timeval *tv;
{
	register unsigned long ticks;
	register long sec, usec;
	int s;

	/*
	 * If the number of usecs in the whole seconds part of the time
	 * difference fits in a long, then the total number of usecs will
	 * fit in an unsigned long.  Compute the total and convert it to
	 * ticks, rounding up and adding 1 to allow for the current tick
	 * to expire.  Rounding also depends on unsigned long arithmetic
	 * to avoid overflow.
	 *
	 * Otherwise, if the number of ticks in the whole seconds part of
	 * the time difference fits in a long, then convert the parts to
	 * ticks separately and add, using similar rounding methods and
	 * overflow avoidance.  This method would work in the previous
	 * case but it is slightly slower and assumes that hz is integral.
	 *
	 * Otherwise, round the time difference down to the maximum
	 * representable value.
	 *
	 * If ints have 32 bits, then the maximum value for any timeout in
	 * 10ms ticks is 248 days.
	 */
	s = splclock();
	sec = tv->tv_sec - time.tv_sec;
	usec = tv->tv_usec - time.tv_usec;
	splx(s);
	if (usec < 0) {
		sec--;
		usec += 1000000;
	}
	if (sec < 0) {
#ifdef DIAGNOSTIC
		printf("hzto: negative time difference %ld sec %ld usec\n",
		       sec, usec);
#endif
		ticks = 1;
	} else if (sec <= LONG_MAX / 1000000)
		ticks = (sec * 1000000 + (unsigned long)usec + (tick - 1))
			/ tick + 1;
	else if (sec <= LONG_MAX / hz)
		ticks = sec * hz
			+ ((unsigned long)usec + (tick - 1)) / tick + 1;
	else
		ticks = LONG_MAX;
	if (ticks > INT_MAX)
		ticks = INT_MAX;
	return (ticks);
}

/*
 * Start profiling on a process.
 *
 * Kernel profiling passes proc0 which never exits and hence
 * keeps the profile clock running constantly.
 */
void
startprofclock(p)
	register struct proc *p;
{
	int s;

	if ((p->p_flag & P_PROFIL) == 0) {
		p->p_flag |= P_PROFIL;
		if (++profprocs == 1 && stathz != 0) {
			s = splstatclock();
			psdiv = pscnt = psratio;
			setstatclockrate(profhz);
			splx(s);
		}
	}
}

/*
 * Stop profiling on a process.
 */
void
stopprofclock(p)
	register struct proc *p;
{
	int s;

	if (p->p_flag & P_PROFIL) {
		p->p_flag &= ~P_PROFIL;
		if (--profprocs == 0 && stathz != 0) {
			s = splstatclock();
			psdiv = pscnt = 1;
			setstatclockrate(stathz);
			splx(s);
		}
	}
}

/*
 * Statistics clock.  Grab profile sample, and if divider reaches 0,
 * do process and kernel statistics.
 */
void
statclock(frame)
	register struct clockframe *frame;
{
#ifdef GPROF
	register struct gmonparam *g;
#endif
	register struct proc *p = curproc;
	register int i;

	if (p) {
		struct pstats *pstats;
		struct rusage *ru;
		struct vmspace *vm;

		/* bump the resource usage of integral space use */
		if ((pstats = p->p_stats) && (ru = &pstats->p_ru) && (vm = p->p_vmspace)) {
			ru->ru_ixrss += vm->vm_tsize * PAGE_SIZE / 1024;
			ru->ru_idrss += vm->vm_dsize * PAGE_SIZE / 1024;
			ru->ru_isrss += vm->vm_ssize * PAGE_SIZE / 1024;
			if ((vm->vm_pmap.pm_stats.resident_count * PAGE_SIZE / 1024) >
			    ru->ru_maxrss) {
				ru->ru_maxrss =
				    vm->vm_pmap.pm_stats.resident_count * PAGE_SIZE / 1024;
			}
        	}
	}

	if (CLKF_USERMODE(frame)) {
		if (p->p_flag & P_PROFIL)
			addupc_intr(p, CLKF_PC(frame), 1);
		if (--pscnt > 0)
			return;
		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled record the tick.
		 */
		p->p_uticks++;
		if (p->p_nice > NZERO)
			cp_time[CP_NICE]++;
		else
			cp_time[CP_USER]++;
	} else {
#ifdef GPROF
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON) {
			i = CLKF_PC(frame) - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
		if (--pscnt > 0)
			return;
		/*
		 * Came from kernel mode, so we were:
		 * - handling an interrupt,
		 * - doing syscall or trap work on behalf of the current
		 *   user process, or
		 * - spinning in the idle loop.
		 * Whichever it is, charge the time as appropriate.
		 * Note that we charge interrupts to the current process,
		 * regardless of whether they are ``for'' that process,
		 * so that we know how much of its real time was spent
		 * in ``non-process'' (i.e., interrupt) work.
		 */
		if (CLKF_INTR(frame)) {
			if (p != NULL)
				p->p_iticks++;
			cp_time[CP_INTR]++;
		} else if (p != NULL) {
			p->p_sticks++;
			cp_time[CP_SYS]++;
		} else
			cp_time[CP_IDLE]++;
	}
	pscnt = psdiv;

	/*
	 * We maintain statistics shown by user-level statistics
	 * programs:  the amount of time in each cpu state, and
	 * the amount of time each of DK_NDRIVE ``drives'' is busy.
	 *
	 * XXX	should either run linked list of drives, or (better)
	 *	grab timestamps in the start & done code.
	 */
	for (i = 0; i < DK_NDRIVE; i++)
		if (dk_busy & (1 << i))
			dk_time[i]++;

	/*
	 * We adjust the priority of the current process.  The priority of
	 * a process gets worse as it accumulates CPU time.  The cpu usage
	 * estimator (p_estcpu) is increased here.  The formula for computing
	 * priorities (in kern_synch.c) will compute a different value each
	 * time p_estcpu increases by 4.  The cpu usage estimator ramps up
	 * quite quickly when the process is running (linearly), and decays
	 * away exponentially, at a rate which is proportionally slower when
	 * the system is busy.  The basic principal is that the system will
	 * 90% forget that the process used a lot of CPU time in 5 * loadav
	 * seconds.  This causes the system to favor processes which haven't
	 * run much recently, and to round-robin among other processes.
	 */
	if (p != NULL) {
		p->p_cpticks++;
		if (++p->p_estcpu == 0)
			p->p_estcpu--;
		if ((p->p_estcpu & 3) == 0) {
			resetpriority(p);
			if (p->p_priority >= PUSER)
				p->p_priority = p->p_usrpri;
		}
	}
}

/*
 * Return information about system clocks.
 */
static int
sysctl_kern_clockrate SYSCTL_HANDLER_ARGS
{
	struct clockinfo clkinfo;
	/*
	 * Construct clockinfo structure.
	 */
	clkinfo.hz = hz;
	clkinfo.tick = tick;
	clkinfo.profhz = profhz;
	clkinfo.stathz = stathz ? stathz : hz;
	return (sysctl_handle_opaque(oidp, &clkinfo, sizeof clkinfo, req));
}

SYSCTL_OID(_kern, KERN_CLOCKRATE, clockrate,
	CTLTYPE_STRUCT|CTLFLAG_RD, 0, 0, sysctl_kern_clockrate, "");

/*#ifdef PPS_SYNC*/
#if 0
/* This code is completely bogus; if anybody ever wants to use it, get
 * the current version from Dave Mills. */

/*
 * hardpps() - discipline CPU clock oscillator to external pps signal
 *
 * This routine is called at each PPS interrupt in order to discipline
 * the CPU clock oscillator to the PPS signal. It integrates successive
 * phase differences between the two oscillators and calculates the
 * frequency offset. This is used in hardclock() to discipline the CPU
 * clock oscillator so that intrinsic frequency error is cancelled out.
 * The code requires the caller to capture the time and hardware
 * counter value at the designated PPS signal transition.
 */
void
hardpps(tvp, usec)
	struct timeval *tvp;		/* time at PPS */
	long usec;			/* hardware counter at PPS */
{
	long u_usec, v_usec, bigtick;
	long cal_sec, cal_usec;

	/*
	 * During the calibration interval adjust the starting time when
	 * the tick overflows. At the end of the interval compute the
	 * duration of the interval and the difference of the hardware
	 * counters at the beginning and end of the interval. This code
	 * is deliciously complicated by the fact valid differences may
	 * exceed the value of tick when using long calibration
	 * intervals and small ticks. Note that the counter can be
	 * greater than tick if caught at just the wrong instant, but
	 * the values returned and used here are correct.
	 */
	bigtick = (long)tick << SHIFT_USEC;
	pps_usec -= ntp_pll.ybar;
	if (pps_usec >= bigtick)
		pps_usec -= bigtick;
	if (pps_usec < 0)
		pps_usec += bigtick;
	pps_time.tv_sec++;
	pps_count++;
	if (pps_count < (1 << pps_shift))
		return;
	pps_count = 0;
	ntp_pll.calcnt++;
	u_usec = usec << SHIFT_USEC;
	v_usec = pps_usec - u_usec;
	if (v_usec >= bigtick >> 1)
		v_usec -= bigtick;
	if (v_usec < -(bigtick >> 1))
		v_usec += bigtick;
	if (v_usec < 0)
		v_usec = -(-v_usec >> ntp_pll.shift);
	else
		v_usec = v_usec >> ntp_pll.shift;
	pps_usec = u_usec;
	cal_sec = tvp->tv_sec;
	cal_usec = tvp->tv_usec;
	cal_sec -= pps_time.tv_sec;
	cal_usec -= pps_time.tv_usec;
	if (cal_usec < 0) {
		cal_usec += 1000000;
		cal_sec--;
	}
	pps_time = *tvp;

	/*
	 * Check for lost interrupts, noise, excessive jitter and
	 * excessive frequency error. The number of timer ticks during
	 * the interval may vary +-1 tick. Add to this a margin of one
	 * tick for the PPS signal jitter and maximum frequency
	 * deviation. If the limits are exceeded, the calibration
	 * interval is reset to the minimum and we start over.
	 */
	u_usec = (long)tick << 1;
	if (!((cal_sec == -1 && cal_usec > (1000000 - u_usec))
	    || (cal_sec == 0 && cal_usec < u_usec))
	    || v_usec > ntp_pll.tolerance || v_usec < -ntp_pll.tolerance) {
		ntp_pll.jitcnt++;
		ntp_pll.shift = NTP_PLL.SHIFT;
		pps_dispinc = PPS_DISPINC;
		ntp_pll.intcnt = 0;
		return;
	}

	/*
	 * A three-stage median filter is used to help deglitch the pps
	 * signal. The median sample becomes the offset estimate; the
	 * difference between the other two samples becomes the
	 * dispersion estimate.
	 */
	pps_mf[2] = pps_mf[1];
	pps_mf[1] = pps_mf[0];
	pps_mf[0] = v_usec;
	if (pps_mf[0] > pps_mf[1]) {
		if (pps_mf[1] > pps_mf[2]) {
			u_usec = pps_mf[1];		/* 0 1 2 */
			v_usec = pps_mf[0] - pps_mf[2];
		} else if (pps_mf[2] > pps_mf[0]) {
			u_usec = pps_mf[0];		/* 2 0 1 */
			v_usec = pps_mf[2] - pps_mf[1];
		} else {
			u_usec = pps_mf[2];		/* 0 2 1 */
			v_usec = pps_mf[0] - pps_mf[1];
		}
	} else {
		if (pps_mf[1] < pps_mf[2]) {
			u_usec = pps_mf[1];		/* 2 1 0 */
			v_usec = pps_mf[2] - pps_mf[0];
		} else  if (pps_mf[2] < pps_mf[0]) {
			u_usec = pps_mf[0];		/* 1 0 2 */
			v_usec = pps_mf[1] - pps_mf[2];
		} else {
			u_usec = pps_mf[2];		/* 1 2 0 */
			v_usec = pps_mf[1] - pps_mf[0];
		}
	}

	/*
	 * Here the dispersion average is updated. If it is less than
	 * the threshold pps_dispmax, the frequency average is updated
	 * as well, but clamped to the tolerance.
	 */
	v_usec = (v_usec >> 1) - ntp_pll.disp;
	if (v_usec < 0)
		ntp_pll.disp -= -v_usec >> PPS_AVG;
	else
		ntp_pll.disp += v_usec >> PPS_AVG;
	if (ntp_pll.disp > pps_dispmax) {
		ntp_pll.discnt++;
		return;
	}
	if (u_usec < 0) {
		ntp_pll.ybar -= -u_usec >> PPS_AVG;
		if (ntp_pll.ybar < -ntp_pll.tolerance)
			ntp_pll.ybar = -ntp_pll.tolerance;
		u_usec = -u_usec;
	} else {
		ntp_pll.ybar += u_usec >> PPS_AVG;
		if (ntp_pll.ybar > ntp_pll.tolerance)
			ntp_pll.ybar = ntp_pll.tolerance;
	}

	/*
	 * Here the calibration interval is adjusted. If the maximum
	 * time difference is greater than tick/4, reduce the interval
	 * by half. If this is not the case for four consecutive
	 * intervals, double the interval.
	 */
	if (u_usec << ntp_pll.shift > bigtick >> 2) {
		ntp_pll.intcnt = 0;
		if (ntp_pll.shift > NTP_PLL.SHIFT) {
			ntp_pll.shift--;
			pps_dispinc <<= 1;
		}
	} else if (ntp_pll.intcnt >= 4) {
		ntp_pll.intcnt = 0;
		if (ntp_pll.shift < NTP_PLL.SHIFTMAX) {
			ntp_pll.shift++;
			pps_dispinc >>= 1;
		}
	} else
		ntp_pll.intcnt++;
}
#endif /* PPS_SYNC */
