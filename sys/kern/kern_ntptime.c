/***********************************************************************
 *								       *
 * Copyright (c) David L. Mills 1993-2001			       *
 *								       *
 * Permission to use, copy, modify, and distribute this software and   *
 * its documentation for any purpose and without fee is hereby	       *
 * granted, provided that the above copyright notice appears in all    *
 * copies and that both the copyright notice and this permission       *
 * notice appear in supporting documentation, and that the name	       *
 * University of Delaware not be used in advertising or publicity      *
 * pertaining to distribution of the software without specific,	       *
 * written prior permission. The University of Delaware makes no       *
 * representations about the suitability this software for any	       *
 * purpose. It is provided "as is" without express or implied	       *
 * warranty.							       *
 *								       *
 **********************************************************************/

/*
 * Adapted from the original sources for FreeBSD and timecounters by:
 * Poul-Henning Kamp <phk@FreeBSD.org>.
 *
 * The 32bit version of the "LP" macros seems a bit past its "sell by" 
 * date so I have retained only the 64bit version and included it directly
 * in this file.
 *
 * Only minor changes done to interface with the timecounters over in
 * sys/kern/kern_clock.c.   Some of the comments below may be (even more)
 * confusing and/or plain wrong in that context.
 *
 * $FreeBSD$
 */

#include "opt_ntp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <sys/timetc.h>
#include <sys/timepps.h>
#include <sys/sysctl.h>

/*
 * Single-precision macros for 64-bit machines
 */
typedef long long l_fp;
#define L_ADD(v, u)	((v) += (u))
#define L_SUB(v, u)	((v) -= (u))
#define L_ADDHI(v, a)	((v) += (long long)(a) << 32)
#define L_NEG(v)	((v) = -(v))
#define L_RSHIFT(v, n) \
	do { \
		if ((v) < 0) \
			(v) = -(-(v) >> (n)); \
		else \
			(v) = (v) >> (n); \
	} while (0)
#define L_MPY(v, a)	((v) *= (a))
#define L_CLR(v)	((v) = 0)
#define L_ISNEG(v)	((v) < 0)
#define L_LINT(v, a)	((v) = (long long)(a) << 32)
#define L_GINT(v)	((v) < 0 ? -(-(v) >> 32) : (v) >> 32)

/*
 * Generic NTP kernel interface
 *
 * These routines constitute the Network Time Protocol (NTP) interfaces
 * for user and daemon application programs. The ntp_gettime() routine
 * provides the time, maximum error (synch distance) and estimated error
 * (dispersion) to client user application programs. The ntp_adjtime()
 * routine is used by the NTP daemon to adjust the system clock to an
 * externally derived time. The time offset and related variables set by
 * this routine are used by other routines in this module to adjust the
 * phase and frequency of the clock discipline loop which controls the
 * system clock.
 *
 * When the kernel time is reckoned directly in nanoseconds (NTP_NANO
 * defined), the time at each tick interrupt is derived directly from
 * the kernel time variable. When the kernel time is reckoned in
 * microseconds, (NTP_NANO undefined), the time is derived from the
 * kernel time variable together with a variable representing the
 * leftover nanoseconds at the last tick interrupt. In either case, the
 * current nanosecond time is reckoned from these values plus an
 * interpolated value derived by the clock routines in another
 * architecture-specific module. The interpolation can use either a
 * dedicated counter or a processor cycle counter (PCC) implemented in
 * some architectures.
 *
 * Note that all routines must run at priority splclock or higher.
 */
/*
 * Phase/frequency-lock loop (PLL/FLL) definitions
 *
 * The nanosecond clock discipline uses two variable types, time
 * variables and frequency variables. Both types are represented as 64-
 * bit fixed-point quantities with the decimal point between two 32-bit
 * halves. On a 32-bit machine, each half is represented as a single
 * word and mathematical operations are done using multiple-precision
 * arithmetic. On a 64-bit machine, ordinary computer arithmetic is
 * used.
 *
 * A time variable is a signed 64-bit fixed-point number in ns and
 * fraction. It represents the remaining time offset to be amortized
 * over succeeding tick interrupts. The maximum time offset is about
 * 0.5 s and the resolution is about 2.3e-10 ns.
 *
 *			1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |s s s|			 ns				   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |			    fraction				   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * A frequency variable is a signed 64-bit fixed-point number in ns/s
 * and fraction. It represents the ns and fraction to be added to the
 * kernel time variable at each second. The maximum frequency offset is
 * about +-500000 ns/s and the resolution is about 2.3e-10 ns/s.
 *
 *			1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |s s s s s s s s s s s s s|	          ns/s			   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |			    fraction				   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*
 * The following variables establish the state of the PLL/FLL and the
 * residual time and frequency offset of the local clock.
 */
#define SHIFT_PLL	4		/* PLL loop gain (shift) */
#define SHIFT_FLL	2		/* FLL loop gain (shift) */

static int time_state = TIME_OK;	/* clock state */
static int time_status = STA_UNSYNC;	/* clock status bits */
static long time_tai;			/* TAI offset (s) */
static long time_monitor;		/* last time offset scaled (ns) */
static long time_constant;		/* poll interval (shift) (s) */
static long time_precision = 1;		/* clock precision (ns) */
static long time_maxerror = MAXPHASE / 1000; /* maximum error (us) */
static long time_esterror = MAXPHASE / 1000; /* estimated error (us) */
static long time_reftime;		/* time at last adjustment (s) */
static long time_tick;			/* nanoseconds per tick (ns) */
static l_fp time_offset;		/* time offset (ns) */
static l_fp time_freq;			/* frequency offset (ns/s) */
static l_fp time_adj;			/* tick adjust (ns/s) */

static int64_t time_adjtime;		/* correction from adjtime(2) (usec) */

#ifdef PPS_SYNC
/*
 * The following variables are used when a pulse-per-second (PPS) signal
 * is available and connected via a modem control lead. They establish
 * the engineering parameters of the clock discipline loop when
 * controlled by the PPS signal.
 */
#define PPS_FAVG	2		/* min freq avg interval (s) (shift) */
#define PPS_FAVGDEF	8		/* default freq avg int (s) (shift) */
#define PPS_FAVGMAX	15		/* max freq avg interval (s) (shift) */
#define PPS_PAVG	4		/* phase avg interval (s) (shift) */
#define PPS_VALID	120		/* PPS signal watchdog max (s) */
#define PPS_MAXWANDER	100000		/* max PPS wander (ns/s) */
#define PPS_POPCORN	2		/* popcorn spike threshold (shift) */

static struct timespec pps_tf[3];	/* phase median filter */
static l_fp pps_freq;			/* scaled frequency offset (ns/s) */
static long pps_fcount;			/* frequency accumulator */
static long pps_jitter;			/* nominal jitter (ns) */
static long pps_stabil;			/* nominal stability (scaled ns/s) */
static long pps_lastsec;		/* time at last calibration (s) */
static int pps_valid;			/* signal watchdog counter */
static int pps_shift = PPS_FAVG;	/* interval duration (s) (shift) */
static int pps_shiftmax = PPS_FAVGDEF;	/* max interval duration (s) (shift) */
static int pps_intcnt;			/* wander counter */

/*
 * PPS signal quality monitors
 */
static long pps_calcnt;			/* calibration intervals */
static long pps_jitcnt;			/* jitter limit exceeded */
static long pps_stbcnt;			/* stability limit exceeded */
static long pps_errcnt;			/* calibration errors */
#endif /* PPS_SYNC */
/*
 * End of phase/frequency-lock loop (PLL/FLL) definitions
 */

static void ntp_init(void);
static void hardupdate(long offset);

/*
 * ntp_gettime() - NTP user application interface
 *
 * See the timex.h header file for synopsis and API description. Note
 * that the TAI offset is returned in the ntvtimeval.tai structure
 * member.
 */
static int
ntp_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct ntptimeval ntv;	/* temporary structure */
	struct timespec atv;	/* nanosecond time */

	nanotime(&atv);
	ntv.time.tv_sec = atv.tv_sec;
	ntv.time.tv_nsec = atv.tv_nsec;
	ntv.maxerror = time_maxerror;
	ntv.esterror = time_esterror;
	ntv.tai = time_tai;
	ntv.time_state = time_state;

	/*
	 * Status word error decode. If any of these conditions occur,
	 * an error is returned, instead of the status word. Most
	 * applications will care only about the fact the system clock
	 * may not be trusted, not about the details.
	 *
	 * Hardware or software error
	 */
	if ((time_status & (STA_UNSYNC | STA_CLOCKERR)) ||

	/*
	 * PPS signal lost when either time or frequency synchronization
	 * requested
	 */
	    (time_status & (STA_PPSFREQ | STA_PPSTIME) &&
	    !(time_status & STA_PPSSIGNAL)) ||

	/*
	 * PPS jitter exceeded when time synchronization requested
	 */
	    (time_status & STA_PPSTIME &&
	    time_status & STA_PPSJITTER) ||

	/*
	 * PPS wander exceeded or calibration error when frequency
	 * synchronization requested
	 */
	    (time_status & STA_PPSFREQ &&
	    time_status & (STA_PPSWANDER | STA_PPSERROR)))
		ntv.time_state = TIME_ERROR;
	return (sysctl_handle_opaque(oidp, &ntv, sizeof ntv, req));
}

SYSCTL_NODE(_kern, OID_AUTO, ntp_pll, CTLFLAG_RW, 0, "");
SYSCTL_PROC(_kern_ntp_pll, OID_AUTO, gettime, CTLTYPE_OPAQUE|CTLFLAG_RD,
	0, sizeof(struct ntptimeval) , ntp_sysctl, "S,ntptimeval", "");

#ifdef PPS_SYNC
SYSCTL_INT(_kern_ntp_pll, OID_AUTO, pps_shiftmax, CTLFLAG_RW, &pps_shiftmax, 0, "");
SYSCTL_INT(_kern_ntp_pll, OID_AUTO, pps_shift, CTLFLAG_RW, &pps_shift, 0, "");
SYSCTL_INT(_kern_ntp_pll, OID_AUTO, time_monitor, CTLFLAG_RD, &time_monitor, 0, "");

SYSCTL_OPAQUE(_kern_ntp_pll, OID_AUTO, pps_freq, CTLFLAG_RD, &pps_freq, sizeof(pps_freq), "I", "");
SYSCTL_OPAQUE(_kern_ntp_pll, OID_AUTO, time_freq, CTLFLAG_RD, &time_freq, sizeof(time_freq), "I", "");
#endif
/*
 * ntp_adjtime() - NTP daemon application interface
 *
 * See the timex.h header file for synopsis and API description. Note
 * that the timex.constant structure member has a dual purpose to set
 * the time constant and to set the TAI offset.
 */
#ifndef _SYS_SYSPROTO_H_
struct ntp_adjtime_args {
	struct timex *tp;
};
#endif

/*
 * MPSAFE
 */
int
ntp_adjtime(struct thread *td, struct ntp_adjtime_args *uap)
{
	struct timex ntv;	/* temporary structure */
	long freq;		/* frequency ns/s) */
	int modes;		/* mode bits from structure */
	int s;			/* caller priority */
	int error;

	error = copyin((caddr_t)uap->tp, (caddr_t)&ntv, sizeof(ntv));
	if (error)
		return(error);

	/*
	 * Update selected clock variables - only the superuser can
	 * change anything. Note that there is no error checking here on
	 * the assumption the superuser should know what it is doing.
	 * Note that either the time constant or TAI offset are loaded
	 * from the ntv.constant member, depending on the mode bits. If
	 * the STA_PLL bit in the status word is cleared, the state and
	 * status words are reset to the initial values at boot.
	 */
	mtx_lock(&Giant);
	modes = ntv.modes;
	if (modes)
		error = suser(td);
	if (error)
		goto done2;
	s = splclock();
	if (modes & MOD_MAXERROR)
		time_maxerror = ntv.maxerror;
	if (modes & MOD_ESTERROR)
		time_esterror = ntv.esterror;
	if (modes & MOD_STATUS) {
		if (time_status & STA_PLL && !(ntv.status & STA_PLL)) {
			time_state = TIME_OK;
			time_status = STA_UNSYNC;
#ifdef PPS_SYNC
			pps_shift = PPS_FAVG;
#endif /* PPS_SYNC */
		}
		time_status &= STA_RONLY;
		time_status |= ntv.status & ~STA_RONLY;
	}
	if (modes & MOD_TIMECONST) {
		if (ntv.constant < 0)
			time_constant = 0;
		else if (ntv.constant > MAXTC)
			time_constant = MAXTC;
		else
			time_constant = ntv.constant;
	}
	if (modes & MOD_TAI) {
		if (ntv.constant > 0) /* XXX zero & negative numbers ? */
			time_tai = ntv.constant;
	}
#ifdef PPS_SYNC
	if (modes & MOD_PPSMAX) {
		if (ntv.shift < PPS_FAVG)
			pps_shiftmax = PPS_FAVG;
		else if (ntv.shift > PPS_FAVGMAX)
			pps_shiftmax = PPS_FAVGMAX;
		else
			pps_shiftmax = ntv.shift;
	}
#endif /* PPS_SYNC */
	if (modes & MOD_NANO)
		time_status |= STA_NANO;
	if (modes & MOD_MICRO)
		time_status &= ~STA_NANO;
	if (modes & MOD_CLKB)
		time_status |= STA_CLK;
	if (modes & MOD_CLKA)
		time_status &= ~STA_CLK;
	if (modes & MOD_OFFSET) {
		if (time_status & STA_NANO)
			hardupdate(ntv.offset);
		else
			hardupdate(ntv.offset * 1000);
	}
	if (modes & MOD_FREQUENCY) {
		freq = (ntv.freq * 1000LL) >> 16;
		if (freq > MAXFREQ)
			L_LINT(time_freq, MAXFREQ);
		else if (freq < -MAXFREQ)
			L_LINT(time_freq, -MAXFREQ);
		else
			L_LINT(time_freq, freq);
#ifdef PPS_SYNC
		pps_freq = time_freq;
#endif /* PPS_SYNC */
	}

	/*
	 * Retrieve all clock variables. Note that the TAI offset is
	 * returned only by ntp_gettime();
	 */
	if (time_status & STA_NANO)
		ntv.offset = L_GINT(time_offset);
	else
		ntv.offset = L_GINT(time_offset) / 1000; /* XXX rounding ? */
	ntv.freq = L_GINT((time_freq / 1000LL) << 16);
	ntv.maxerror = time_maxerror;
	ntv.esterror = time_esterror;
	ntv.status = time_status;
	ntv.constant = time_constant;
	if (time_status & STA_NANO)
		ntv.precision = time_precision;
	else
		ntv.precision = time_precision / 1000;
	ntv.tolerance = MAXFREQ * SCALE_PPM;
#ifdef PPS_SYNC
	ntv.shift = pps_shift;
	ntv.ppsfreq = L_GINT((pps_freq / 1000LL) << 16);
	if (time_status & STA_NANO)
		ntv.jitter = pps_jitter;
	else
		ntv.jitter = pps_jitter / 1000;
	ntv.stabil = pps_stabil;
	ntv.calcnt = pps_calcnt;
	ntv.errcnt = pps_errcnt;
	ntv.jitcnt = pps_jitcnt;
	ntv.stbcnt = pps_stbcnt;
#endif /* PPS_SYNC */
	splx(s);

	error = copyout((caddr_t)&ntv, (caddr_t)uap->tp, sizeof(ntv));
	if (error)
		goto done2;

	/*
	 * Status word error decode. See comments in
	 * ntp_gettime() routine.
	 */
	if ((time_status & (STA_UNSYNC | STA_CLOCKERR)) ||
	    (time_status & (STA_PPSFREQ | STA_PPSTIME) &&
	    !(time_status & STA_PPSSIGNAL)) ||
	    (time_status & STA_PPSTIME &&
	    time_status & STA_PPSJITTER) ||
	    (time_status & STA_PPSFREQ &&
	    time_status & (STA_PPSWANDER | STA_PPSERROR))) {
		td->td_retval[0] = TIME_ERROR;
	} else {
		td->td_retval[0] = time_state;
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * second_overflow() - called after ntp_tick_adjust()
 *
 * This routine is ordinarily called immediately following the above
 * routine ntp_tick_adjust(). While these two routines are normally
 * combined, they are separated here only for the purposes of
 * simulation.
 */
void
ntp_update_second(struct timecounter *tcp)
{
	u_int32_t *newsec;
	int tickrate;
	l_fp ftemp;		/* 32/64-bit temporary */

	newsec = &tcp->tc_offset.sec;
	/*
	 * On rollover of the second both the nanosecond and microsecond
	 * clocks are updated and the state machine cranked as
	 * necessary. The phase adjustment to be used for the next
	 * second is calculated and the maximum error is increased by
	 * the tolerance.
	 */
	time_maxerror += MAXFREQ / 1000;

	/*
	 * Leap second processing. If in leap-insert state at
	 * the end of the day, the system clock is set back one
	 * second; if in leap-delete state, the system clock is
	 * set ahead one second. The nano_time() routine or
	 * external clock driver will insure that reported time
	 * is always monotonic.
	 */
	switch (time_state) {

		/*
		 * No warning.
		 */
		case TIME_OK:
		if (time_status & STA_INS)
			time_state = TIME_INS;
		else if (time_status & STA_DEL)
			time_state = TIME_DEL;
		break;

		/*
		 * Insert second 23:59:60 following second
		 * 23:59:59.
		 */
		case TIME_INS:
		if (!(time_status & STA_INS))
			time_state = TIME_OK;
		else if ((*newsec) % 86400 == 0) {
			(*newsec)--;
			time_state = TIME_OOP;
		}
		break;

		/*
		 * Delete second 23:59:59.
		 */
		case TIME_DEL:
		if (!(time_status & STA_DEL))
			time_state = TIME_OK;
		else if (((*newsec) + 1) % 86400 == 0) {
			(*newsec)++;
			time_tai--;
			time_state = TIME_WAIT;
		}
		break;

		/*
		 * Insert second in progress.
		 */
		case TIME_OOP:
			time_tai++;
			time_state = TIME_WAIT;
		break;

		/*
		 * Wait for status bits to clear.
		 */
		case TIME_WAIT:
		if (!(time_status & (STA_INS | STA_DEL)))
			time_state = TIME_OK;
	}

	/*
	 * Compute the total time adjustment for the next second
	 * in ns. The offset is reduced by a factor depending on
	 * whether the PPS signal is operating. Note that the
	 * value is in effect scaled by the clock frequency,
	 * since the adjustment is added at each tick interrupt.
	 */
	ftemp = time_offset;
#ifdef PPS_SYNC
	/* XXX even if PPS signal dies we should finish adjustment ? */
	if (time_status & STA_PPSTIME && time_status &
	    STA_PPSSIGNAL)
		L_RSHIFT(ftemp, pps_shift);
	else
		L_RSHIFT(ftemp, SHIFT_PLL + time_constant);
#else
		L_RSHIFT(ftemp, SHIFT_PLL + time_constant);
#endif /* PPS_SYNC */
	time_adj = ftemp;
	L_SUB(time_offset, ftemp);
	L_ADD(time_adj, time_freq);
	
	/*
	 * Apply any correction from adjtime(2).  If more than one second
	 * off we slew at a rate of 5ms/s (5000 PPM) else 500us/s (500PPM)
	 * until the last second is slewed the final < 500 usecs.
	 */
	if (time_adjtime != 0) {
		if (time_adjtime > 1000000)
			tickrate = 5000;
		else if (time_adjtime < -1000000)
			tickrate = -5000;
		else if (time_adjtime > 500)
			tickrate = 500;
		else if (time_adjtime < -500)
			tickrate = -500;
		else if (time_adjtime != 0)
			tickrate = time_adjtime;
		else
			tickrate = 0;	/* GCC sucks! */
		time_adjtime -= tickrate;
		L_LINT(ftemp, tickrate * 1000);
		L_ADD(time_adj, ftemp);
	}
	tcp->tc_adjustment = time_adj;
		
#ifdef PPS_SYNC
	if (pps_valid > 0)
		pps_valid--;
	else
		time_status &= ~STA_PPSSIGNAL;
#endif /* PPS_SYNC */
}

/*
 * ntp_init() - initialize variables and structures
 *
 * This routine must be called after the kernel variables hz and tick
 * are set or changed and before the next tick interrupt. In this
 * particular implementation, these values are assumed set elsewhere in
 * the kernel. The design allows the clock frequency and tick interval
 * to be changed while the system is running. So, this routine should
 * probably be integrated with the code that does that.
 */
static void
ntp_init()
{

	/*
	 * The following variable must be initialized any time the
	 * kernel variable hz is changed.
	 */
	time_tick = NANOSECOND / hz;

	/*
	 * The following variables are initialized only at startup. Only
	 * those structures not cleared by the compiler need to be
	 * initialized, and these only in the simulator. In the actual
	 * kernel, any nonzero values here will quickly evaporate.
	 */
	L_CLR(time_offset);
	L_CLR(time_freq);
#ifdef PPS_SYNC
	pps_tf[0].tv_sec = pps_tf[0].tv_nsec = 0;
	pps_tf[1].tv_sec = pps_tf[1].tv_nsec = 0;
	pps_tf[2].tv_sec = pps_tf[2].tv_nsec = 0;
	pps_fcount = 0;
	L_CLR(pps_freq);
#endif /* PPS_SYNC */	   
}

SYSINIT(ntpclocks, SI_SUB_CLOCKS, SI_ORDER_FIRST, ntp_init, NULL)

/*
 * hardupdate() - local clock update
 *
 * This routine is called by ntp_adjtime() to update the local clock
 * phase and frequency. The implementation is of an adaptive-parameter,
 * hybrid phase/frequency-lock loop (PLL/FLL). The routine computes new
 * time and frequency offset estimates for each call. If the kernel PPS
 * discipline code is configured (PPS_SYNC), the PPS signal itself
 * determines the new time offset, instead of the calling argument.
 * Presumably, calls to ntp_adjtime() occur only when the caller
 * believes the local clock is valid within some bound (+-128 ms with
 * NTP). If the caller's time is far different than the PPS time, an
 * argument will ensue, and it's not clear who will lose.
 *
 * For uncompensated quartz crystal oscillators and nominal update
 * intervals less than 256 s, operation should be in phase-lock mode,
 * where the loop is disciplined to phase. For update intervals greater
 * than 1024 s, operation should be in frequency-lock mode, where the
 * loop is disciplined to frequency. Between 256 s and 1024 s, the mode
 * is selected by the STA_MODE status bit.
 */
static void
hardupdate(offset)
	long offset;		/* clock offset (ns) */
{
	long mtemp;
	l_fp ftemp;

	/*
	 * Select how the phase is to be controlled and from which
	 * source. If the PPS signal is present and enabled to
	 * discipline the time, the PPS offset is used; otherwise, the
	 * argument offset is used.
	 */
	if (!(time_status & STA_PLL))
		return;
	if (!(time_status & STA_PPSTIME && time_status &
	    STA_PPSSIGNAL)) {
		if (offset > MAXPHASE)
			time_monitor = MAXPHASE;
		else if (offset < -MAXPHASE)
			time_monitor = -MAXPHASE;
		else
			time_monitor = offset;
		L_LINT(time_offset, time_monitor);
	}

	/*
	 * Select how the frequency is to be controlled and in which
	 * mode (PLL or FLL). If the PPS signal is present and enabled
	 * to discipline the frequency, the PPS frequency is used;
	 * otherwise, the argument offset is used to compute it.
	 */
	if (time_status & STA_PPSFREQ && time_status & STA_PPSSIGNAL) {
		time_reftime = time_second;
		return;
	}
	if (time_status & STA_FREQHOLD || time_reftime == 0)
		time_reftime = time_second;
	mtemp = time_second - time_reftime;
	L_LINT(ftemp, time_monitor);
	L_RSHIFT(ftemp, (SHIFT_PLL + 2 + time_constant) << 1);
	L_MPY(ftemp, mtemp);
	L_ADD(time_freq, ftemp);
	time_status &= ~STA_MODE;
	if (mtemp >= MINSEC && (time_status & STA_FLL || mtemp >
	    MAXSEC)) {
		L_LINT(ftemp, (time_monitor << 4) / mtemp);
		L_RSHIFT(ftemp, SHIFT_FLL + 4);
		L_ADD(time_freq, ftemp);
		time_status |= STA_MODE;
	}
	time_reftime = time_second;
	if (L_GINT(time_freq) > MAXFREQ)
		L_LINT(time_freq, MAXFREQ);
	else if (L_GINT(time_freq) < -MAXFREQ)
		L_LINT(time_freq, -MAXFREQ);
}

#ifdef PPS_SYNC
/*
 * hardpps() - discipline CPU clock oscillator to external PPS signal
 *
 * This routine is called at each PPS interrupt in order to discipline
 * the CPU clock oscillator to the PPS signal. There are two independent
 * first-order feedback loops, one for the phase, the other for the
 * frequency. The phase loop measures and grooms the PPS phase offset
 * and leaves it in a handy spot for the seconds overflow routine. The
 * frequency loop averages successive PPS phase differences and
 * calculates the PPS frequency offset, which is also processed by the
 * seconds overflow routine. The code requires the caller to capture the
 * time and architecture-dependent hardware counter values in
 * nanoseconds at the on-time PPS signal transition.
 *
 * Note that, on some Unix systems this routine runs at an interrupt
 * priority level higher than the timer interrupt routine hardclock().
 * Therefore, the variables used are distinct from the hardclock()
 * variables, except for the actual time and frequency variables, which
 * are determined by this routine and updated atomically.
 */
void
hardpps(tsp, nsec)
	struct timespec *tsp;	/* time at PPS */
	long nsec;		/* hardware counter at PPS */
{
	long u_sec, u_nsec, v_nsec; /* temps */
	l_fp ftemp;

	/*
	 * The signal is first processed by a range gate and frequency
	 * discriminator. The range gate rejects noise spikes outside
	 * the range +-500 us. The frequency discriminator rejects input
	 * signals with apparent frequency outside the range 1 +-500
	 * PPM. If two hits occur in the same second, we ignore the
	 * later hit; if not and a hit occurs outside the range gate,
	 * keep the later hit for later comparison, but do not process
	 * it.
	 */
	time_status |= STA_PPSSIGNAL | STA_PPSJITTER;
	time_status &= ~(STA_PPSWANDER | STA_PPSERROR);
	pps_valid = PPS_VALID;
	u_sec = tsp->tv_sec;
	u_nsec = tsp->tv_nsec;
	if (u_nsec >= (NANOSECOND >> 1)) {
		u_nsec -= NANOSECOND;
		u_sec++;
	}
	v_nsec = u_nsec - pps_tf[0].tv_nsec;
	if (u_sec == pps_tf[0].tv_sec && v_nsec < NANOSECOND -
	    MAXFREQ)
		return;
	pps_tf[2] = pps_tf[1];
	pps_tf[1] = pps_tf[0];
	pps_tf[0].tv_sec = u_sec;
	pps_tf[0].tv_nsec = u_nsec;

	/*
	 * Compute the difference between the current and previous
	 * counter values. If the difference exceeds 0.5 s, assume it
	 * has wrapped around, so correct 1.0 s. If the result exceeds
	 * the tick interval, the sample point has crossed a tick
	 * boundary during the last second, so correct the tick. Very
	 * intricate.
	 */
	u_nsec = nsec;
	if (u_nsec > (NANOSECOND >> 1))
		u_nsec -= NANOSECOND;
	else if (u_nsec < -(NANOSECOND >> 1))
		u_nsec += NANOSECOND;
	pps_fcount += u_nsec;
	if (v_nsec > MAXFREQ || v_nsec < -MAXFREQ)
		return;
	time_status &= ~STA_PPSJITTER;

	/*
	 * A three-stage median filter is used to help denoise the PPS
	 * time. The median sample becomes the time offset estimate; the
	 * difference between the other two samples becomes the time
	 * dispersion (jitter) estimate.
	 */
	if (pps_tf[0].tv_nsec > pps_tf[1].tv_nsec) {
		if (pps_tf[1].tv_nsec > pps_tf[2].tv_nsec) {
			v_nsec = pps_tf[1].tv_nsec;	/* 0 1 2 */
			u_nsec = pps_tf[0].tv_nsec - pps_tf[2].tv_nsec;
		} else if (pps_tf[2].tv_nsec > pps_tf[0].tv_nsec) {
			v_nsec = pps_tf[0].tv_nsec;	/* 2 0 1 */
			u_nsec = pps_tf[2].tv_nsec - pps_tf[1].tv_nsec;
		} else {
			v_nsec = pps_tf[2].tv_nsec;	/* 0 2 1 */
			u_nsec = pps_tf[0].tv_nsec - pps_tf[1].tv_nsec;
		}
	} else {
		if (pps_tf[1].tv_nsec < pps_tf[2].tv_nsec) {
			v_nsec = pps_tf[1].tv_nsec;	/* 2 1 0 */
			u_nsec = pps_tf[2].tv_nsec - pps_tf[0].tv_nsec;
		} else if (pps_tf[2].tv_nsec < pps_tf[0].tv_nsec) {
			v_nsec = pps_tf[0].tv_nsec;	/* 1 0 2 */
			u_nsec = pps_tf[1].tv_nsec - pps_tf[2].tv_nsec;
		} else {
			v_nsec = pps_tf[2].tv_nsec;	/* 1 2 0 */
			u_nsec = pps_tf[1].tv_nsec - pps_tf[0].tv_nsec;
		}
	}

	/*
	 * Nominal jitter is due to PPS signal noise and interrupt
	 * latency. If it exceeds the popcorn threshold, the sample is
	 * discarded. otherwise, if so enabled, the time offset is
	 * updated. We can tolerate a modest loss of data here without
	 * much degrading time accuracy.
	 */
	if (u_nsec > (pps_jitter << PPS_POPCORN)) {
		time_status |= STA_PPSJITTER;
		pps_jitcnt++;
	} else if (time_status & STA_PPSTIME) {
		time_monitor = -v_nsec;
		L_LINT(time_offset, time_monitor);
	}
	pps_jitter += (u_nsec - pps_jitter) >> PPS_FAVG;
	u_sec = pps_tf[0].tv_sec - pps_lastsec;
	if (u_sec < (1 << pps_shift))
		return;

	/*
	 * At the end of the calibration interval the difference between
	 * the first and last counter values becomes the scaled
	 * frequency. It will later be divided by the length of the
	 * interval to determine the frequency update. If the frequency
	 * exceeds a sanity threshold, or if the actual calibration
	 * interval is not equal to the expected length, the data are
	 * discarded. We can tolerate a modest loss of data here without
	 * much degrading frequency accuracy.
	 */
	pps_calcnt++;
	v_nsec = -pps_fcount;
	pps_lastsec = pps_tf[0].tv_sec;
	pps_fcount = 0;
	u_nsec = MAXFREQ << pps_shift;
	if (v_nsec > u_nsec || v_nsec < -u_nsec || u_sec != (1 <<
	    pps_shift)) {
		time_status |= STA_PPSERROR;
		pps_errcnt++;
		return;
	}

	/*
	 * Here the raw frequency offset and wander (stability) is
	 * calculated. If the wander is less than the wander threshold
	 * for four consecutive averaging intervals, the interval is
	 * doubled; if it is greater than the threshold for four
	 * consecutive intervals, the interval is halved. The scaled
	 * frequency offset is converted to frequency offset. The
	 * stability metric is calculated as the average of recent
	 * frequency changes, but is used only for performance
	 * monitoring.
	 */
	L_LINT(ftemp, v_nsec);
	L_RSHIFT(ftemp, pps_shift);
	L_SUB(ftemp, pps_freq);
	u_nsec = L_GINT(ftemp);
	if (u_nsec > PPS_MAXWANDER) {
		L_LINT(ftemp, PPS_MAXWANDER);
		pps_intcnt--;
		time_status |= STA_PPSWANDER;
		pps_stbcnt++;
	} else if (u_nsec < -PPS_MAXWANDER) {
		L_LINT(ftemp, -PPS_MAXWANDER);
		pps_intcnt--;
		time_status |= STA_PPSWANDER;
		pps_stbcnt++;
	} else {
		pps_intcnt++;
	}
	if (pps_intcnt >= 4) {
		pps_intcnt = 4;
		if (pps_shift < pps_shiftmax) {
			pps_shift++;
			pps_intcnt = 0;
		}
	} else if (pps_intcnt <= -4 || pps_shift > pps_shiftmax) {
		pps_intcnt = -4;
		if (pps_shift > PPS_FAVG) {
			pps_shift--;
			pps_intcnt = 0;
		}
	}
	if (u_nsec < 0)
		u_nsec = -u_nsec;
	pps_stabil += (u_nsec * SCALE_PPM - pps_stabil) >> PPS_FAVG;

	/*
	 * The PPS frequency is recalculated and clamped to the maximum
	 * MAXFREQ. If enabled, the system clock frequency is updated as
	 * well.
	 */
	L_ADD(pps_freq, ftemp);
	u_nsec = L_GINT(pps_freq);
	if (u_nsec > MAXFREQ)
		L_LINT(pps_freq, MAXFREQ);
	else if (u_nsec < -MAXFREQ)
		L_LINT(pps_freq, -MAXFREQ);
	if (time_status & STA_PPSFREQ)
		time_freq = pps_freq;
}
#endif /* PPS_SYNC */

#ifndef _SYS_SYSPROTO_H_
struct adjtime_args {
	struct timeval *delta;
	struct timeval *olddelta;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
adjtime(struct thread *td, struct adjtime_args *uap)
{
	struct timeval atv;
	int error;

	mtx_lock(&Giant);

	if ((error = suser(td)))
		goto done2;
	if (uap->olddelta) {
		atv.tv_sec = time_adjtime / 1000000;
		atv.tv_usec = time_adjtime % 1000000;
		if (atv.tv_usec < 0) {
			atv.tv_usec += 1000000;
			atv.tv_sec--;
		}
		printf("Old: time_adjtime = %ld.%06ld %lld\n", 
		    atv.tv_sec, atv.tv_usec, (long long)time_adjtime);
		error = copyout(&atv, uap->olddelta, sizeof(atv));
		if (error)
			goto done2;
	}
	if (uap->delta) {
		error = copyin(uap->delta, &atv, sizeof(atv));
		if (error)
			goto done2;
		time_adjtime = (int64_t)atv.tv_sec * 1000000 + atv.tv_usec;
		printf("New: time_adjtime = %ld.%06ld %lld\n", 
		    atv.tv_sec, atv.tv_usec, (long long)time_adjtime);
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}

