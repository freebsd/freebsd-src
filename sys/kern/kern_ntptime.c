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
 ******************************************************************************/

/*
 * Modification history kern_ntptime.c
 *
 * 24 Sep 94	David L. Mills
 *	Tightened code at exits.
 *
 * 24 Mar 94	David L. Mills
 *	Revised syscall interface to include new variables for PPS
 *	time discipline.
 *
 * 14 Feb 94	David L. Mills
 *	Added code for external clock
 *
 * 28 Nov 93	David L. Mills
 *	Revised frequency scaling to conform with adjusted parameters
 *
 * 17 Sep 93	David L. Mills
 *	Created file
 */
/*
 * ntp_gettime(), ntp_adjtime() - precision time interface for SunOS
 * V4.1.1 and V4.1.3
 *
 * These routines consitute the Network Time Protocol (NTP) interfaces
 * for user and daemon application programs. The ntp_gettime() routine
 * provides the time, maximum error (synch distance) and estimated error
 * (dispersion) to client user application programs. The ntp_adjtime()
 * routine is used by the NTP daemon to adjust the system clock to an
 * externally derived time. The time offset and related variables set by
 * this routine are used by hardclock() to adjust the phase and
 * frequency of the phase-lock loop which controls the system clock.
 */

#include "opt_ntp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/timex.h>
#include <sys/sysctl.h>

/*
 * Phase/frequency-lock loop (PLL/FLL) definitions
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
 * time_offset is used by the PLL/FLL to adjust the system time in small
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
static int time_status = STA_UNSYNC;	/* clock status bits */
static int time_state = TIME_OK;	/* clock state */
static long time_offset = 0;		/* time offset (us) */
static long time_constant = 0;		/* pll time constant */
static long time_tolerance = MAXFREQ;	/* frequency tolerance (scaled ppm) */
static long time_precision = 1;		/* clock precision (us) */
static long time_maxerror = MAXPHASE;	/* maximum error (us) */
static long time_esterror = MAXPHASE;	/* estimated error (us) */
static int time_daemon = 0;		/* No timedaemon active */

/*
 * The following variables establish the state of the PLL/FLL and the
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
 * interrupt and is recomputed from time_phase and time_freq at each
 * seconds rollover.
 *
 * time_reftime is the second's portion of the system time on the last
 * call to ntp_adjtime(). It is used to adjust the time_freq variable
 * and to increase the time_maxerror as the time since last update
 * increases.
 */
long time_phase = 0;			/* phase offset (scaled us) */
static long time_freq = 0;		/* frequency offset (scaled ppm) */
long time_adj = 0;			/* tick adjust (scaled 1 / hz) */
static long time_reftime = 0;		/* time at last adjustment (s) */

#ifdef PPS_SYNC
/*
 * The following variables are used only if the kernel PPS discipline
 * code is configured (PPS_SYNC). The scale factors are defined in the
 * timex.h header file.
 *
 * pps_time contains the time at each calibration interval, as read by
 * microtime(). pps_count counts the seconds of the calibration
 * interval, the duration of which is nominally pps_shift in powers of
 * two.
 *
 * pps_offset is the time offset produced by the time median filter
 * pps_tf[], while pps_jitter is the dispersion (jitter) measured by
 * this filter.
 *
 * pps_freq is the frequency offset produced by the frequency median
 * filter pps_ff[], while pps_stabil is the dispersion (wander) measured
 * by this filter.
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
 * pps_intcnt counts the calibration intervals for use in the interval-
 * adaptation algorithm. It's just too complicated for words.
 */
static struct timeval pps_time;	/* kernel time at last interval */
static long pps_offset = 0;		/* pps time offset (us) */
static long pps_jitter = MAXTIME;	/* pps time dispersion (jitter) (us) */
static long pps_tf[] = {0, 0, 0};	/* pps time offset median filter (us) */
static long pps_freq = 0;		/* frequency offset (scaled ppm) */
static long pps_stabil = MAXFREQ;	/* frequency dispersion (scaled ppm) */
static long pps_ff[] = {0, 0, 0};	/* frequency offset median filter */
static long pps_usec = 0;		/* microsec counter at last interval */
static long pps_valid = PPS_VALID;	/* pps signal watchdog counter */
static int pps_glitch = 0;		/* pps signal glitch counter */
static int pps_count = 0;		/* calibration interval counter (s) */
static int pps_shift = PPS_SHIFT;	/* interval duration (s) (shift) */
static int pps_intcnt = 0;		/* intervals at current duration */

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
static long pps_jitcnt = 0;		/* jitter limit exceeded */
static long pps_calcnt = 0;		/* calibration intervals */
static long pps_errcnt = 0;		/* calibration errors */
static long pps_stbcnt = 0;		/* stability limit exceeded */
#endif /* PPS_SYNC */

static void hardupdate __P((long offset));

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
 * For uncompensated quartz crystal oscillatores and nominal update
 * intervals less than 1024 s, operation should be in phase-lock mode
 * (STA_FLL = 0), where the loop is disciplined to phase. For update
 * intervals greater than thiss, operation should be in frequency-lock
 * mode (STA_FLL = 1), where the loop is disciplined to frequency.
 *
 * Note: splclock() is in effect.
 */
static void
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

	/*
	 * Scale the phase adjustment and clamp to the operating range.
	 */
	if (ltemp > MAXPHASE)
		time_offset = MAXPHASE << SHIFT_UPDATE;
	else if (ltemp < -MAXPHASE)
		time_offset = -(MAXPHASE << SHIFT_UPDATE);
	else
		time_offset = ltemp << SHIFT_UPDATE;

	/*
	 * Select whether the frequency is to be controlled and in which
	 * mode (PLL or FLL). Clamp to the operating range. Ugly
	 * multiply/divide should be replaced someday.
	 */
	if (time_status & STA_FREQHOLD || time_reftime == 0)
		time_reftime = time_second;
	mtemp = time_second - time_reftime;
	time_reftime = time_second;
	if (time_status & STA_FLL) {
		if (mtemp >= MINSEC) {
			ltemp = ((time_offset / mtemp) << (SHIFT_USEC -
			    SHIFT_UPDATE));
			if (ltemp < 0)
				time_freq -= -ltemp >> SHIFT_KH;
			else
				time_freq += ltemp >> SHIFT_KH;
		}
	} else {
		if (mtemp < MAXSEC) {
			ltemp *= mtemp;
			if (ltemp < 0)
				time_freq -= -ltemp >> (time_constant +
				    time_constant + SHIFT_KF -
				    SHIFT_USEC);
			else
				time_freq += ltemp >> (time_constant +
				    time_constant + SHIFT_KF -
				    SHIFT_USEC);
		}
	}
	if (time_freq > time_tolerance)
		time_freq = time_tolerance;
	else if (time_freq < -time_tolerance)
		time_freq = -time_tolerance;
}

/*
 * On rollover of the second the phase adjustment to be used for
 * the next second is calculated. Also, the maximum error is
 * increased by the tolerance. If the PPS frequency discipline
 * code is present, the phase is increased to compensate for the
 * CPU clock oscillator frequency error.
 *
 * On a 32-bit machine and given parameters in the timex.h
 * header file, the maximum phase adjustment is +-512 ms and
 * maximum frequency offset is a tad less than) +-512 ppm. On a
 * 64-bit machine, you shouldn't need to ask.
 */
void
ntp_update_second(struct timecounter *tc)
{
	u_int32_t *newsec;
	long ltemp;

	if (!time_daemon)
		return;

	newsec = &tc->offset_sec;
	time_maxerror += time_tolerance >> SHIFT_USEC;

	/*
	* Compute the phase adjustment for the next second. In
	* PLL mode, the offset is reduced by a fixed factor
	* times the time constant. In FLL mode the offset is
	* used directly. In either mode, the maximum phase
	* adjustment for each second is clamped so as to spread
	* the adjustment over not more than the number of
	* seconds between updates.
	*/
	if (time_offset < 0) {
		ltemp = -time_offset;
		if (!(time_status & STA_FLL))
			ltemp >>= SHIFT_KG + time_constant;
		if (ltemp > (MAXPHASE / MINSEC) << SHIFT_UPDATE)
			ltemp = (MAXPHASE / MINSEC) << SHIFT_UPDATE;
		time_offset += ltemp;
		time_adj = -ltemp << (SHIFT_SCALE - SHIFT_UPDATE);
	} else {
		ltemp = time_offset;
		if (!(time_status & STA_FLL))
			ltemp >>= SHIFT_KG + time_constant;
		if (ltemp > (MAXPHASE / MINSEC) << SHIFT_UPDATE)
			ltemp = (MAXPHASE / MINSEC) << SHIFT_UPDATE;
		time_offset -= ltemp;
		time_adj = ltemp << (SHIFT_SCALE - SHIFT_UPDATE);
	}

	/*
	* Compute the frequency estimate and additional phase
	* adjustment due to frequency error for the next
	* second. When the PPS signal is engaged, gnaw on the
	* watchdog counter and update the frequency computed by
	* the pll and the PPS signal.
	*/
#ifdef PPS_SYNC
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
		time_adj -= -ltemp << (SHIFT_SCALE - SHIFT_USEC);
	else
		time_adj += ltemp << (SHIFT_SCALE - SHIFT_USEC);

	tc->adjustment = time_adj;
	
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
			if ((*newsec) % 86400 == 0) {
				(*newsec)--;
				time_state = TIME_OOP;
			}
			break;

		case TIME_DEL:
			if (((*newsec) + 1) % 86400 == 0) {
				(*newsec)++;
				time_state = TIME_WAIT;
			}
			break;

		case TIME_OOP:
			time_state = TIME_WAIT;
			break;

		case TIME_WAIT:
			if (!(time_status & (STA_INS | STA_DEL)))
				time_state = TIME_OK;
			break;
	}
}

static int
ntp_sysctl SYSCTL_HANDLER_ARGS
{
	struct timeval atv;
	struct ntptimeval ntv;
	int s;

	s = splclock();
	microtime(&atv);
	ntv.time = atv;
	ntv.maxerror = time_maxerror;
	ntv.esterror = time_esterror;
	splx(s);

	ntv.time_state = time_state;

	/*
	 * Status word error decode. If any of these conditions
	 * occur, an error is returned, instead of the status
	 * word. Most applications will care only about the fact
	 * the system clock may not be trusted, not about the
	 * details.
	 *
	 * Hardware or software error
	 */
	if (time_status & (STA_UNSYNC | STA_CLOCKERR)) {
		ntv.time_state = TIME_ERROR;
	}

	/*
	 * PPS signal lost when either time or frequency
	 * synchronization requested
	 */
	if (time_status & (STA_PPSFREQ | STA_PPSTIME) &&
	    !(time_status & STA_PPSSIGNAL)) {
		ntv.time_state = TIME_ERROR;
	}

	/*
	 * PPS jitter exceeded when time synchronization
	 * requested
	 */
	if (time_status & STA_PPSTIME &&
	    time_status & STA_PPSJITTER) {
		ntv.time_state = TIME_ERROR;
	}

	/*
	 * PPS wander exceeded or calibration error when
	 * frequency synchronization requested
	 */
	if (time_status & STA_PPSFREQ &&
	    time_status & (STA_PPSWANDER | STA_PPSERROR)) {
		ntv.time_state = TIME_ERROR;
	}
	return (sysctl_handle_opaque(oidp, &ntv, sizeof ntv, req));
}

SYSCTL_NODE(_kern, KERN_NTP_PLL, ntp_pll, CTLFLAG_RW, 0,
	"NTP kernel PLL related stuff");
SYSCTL_PROC(_kern_ntp_pll, NTP_PLL_GETTIME, gettime, CTLTYPE_OPAQUE|CTLFLAG_RD,
	0, sizeof(struct ntptimeval) , ntp_sysctl, "S,ntptimeval", "");

/*
 * ntp_adjtime() - NTP daemon application interface
 */
#ifndef _SYS_SYSPROTO_H_
struct ntp_adjtime_args {
  struct timex *tp;
};
#endif

int
ntp_adjtime(struct proc *p, struct ntp_adjtime_args *uap)
{
	struct timex ntv;
	int modes;
	int s;
	int error;

	time_daemon = 1;

	error = copyin((caddr_t)uap->tp, (caddr_t)&ntv, sizeof(ntv));
	if (error)
		return error;

	/*
	 * Update selected clock variables - only the superuser can
	 * change anything. Note that there is no error checking here on
	 * the assumption the superuser should know what it is doing.
	 */
	modes = ntv.modes;
	if ((modes != 0)
	    && (error = suser(p->p_cred->pc_ucred, &p->p_acflag)))
		return error;

	s = splclock();
	if (modes & MOD_FREQUENCY)
#ifdef PPS_SYNC
		time_freq = ntv.freq - pps_freq;
#else /* PPS_SYNC */
		time_freq = ntv.freq;
#endif /* PPS_SYNC */
	if (modes & MOD_MAXERROR)
		time_maxerror = ntv.maxerror;
	if (modes & MOD_ESTERROR)
		time_esterror = ntv.esterror;
	if (modes & MOD_STATUS) {
		time_status &= STA_RONLY;
		time_status |= ntv.status & ~STA_RONLY;
	}
	if (modes & MOD_TIMECONST)
		time_constant = ntv.constant;
	if (modes & MOD_OFFSET)
		hardupdate(ntv.offset);

	/*
	 * Retrieve all clock variables
	 */
	if (time_offset < 0)
		ntv.offset = -(-time_offset >> SHIFT_UPDATE);
	else
		ntv.offset = time_offset >> SHIFT_UPDATE;
#ifdef PPS_SYNC
	ntv.freq = time_freq + pps_freq;
#else /* PPS_SYNC */
	ntv.freq = time_freq;
#endif /* PPS_SYNC */
	ntv.maxerror = time_maxerror;
	ntv.esterror = time_esterror;
	ntv.status = time_status;
	ntv.constant = time_constant;
	ntv.precision = time_precision;
	ntv.tolerance = time_tolerance;
#ifdef PPS_SYNC
	ntv.shift = pps_shift;
	ntv.ppsfreq = pps_freq;
	ntv.jitter = pps_jitter >> PPS_AVG;
	ntv.stabil = pps_stabil;
	ntv.calcnt = pps_calcnt;
	ntv.errcnt = pps_errcnt;
	ntv.jitcnt = pps_jitcnt;
	ntv.stbcnt = pps_stbcnt;
#endif /* PPS_SYNC */
	(void)splx(s);

	error = copyout((caddr_t)&ntv, (caddr_t)uap->tp, sizeof(ntv));
	if (!error) {
		/*
		 * Status word error decode. See comments in
		 * ntp_gettime() routine.
		 */
		p->p_retval[0] = time_state;
		if (time_status & (STA_UNSYNC | STA_CLOCKERR))
			p->p_retval[0] = TIME_ERROR;
		if (time_status & (STA_PPSFREQ | STA_PPSTIME) &&
		    !(time_status & STA_PPSSIGNAL))
			p->p_retval[0] = TIME_ERROR;
		if (time_status & STA_PPSTIME &&
		    time_status & STA_PPSJITTER)
			p->p_retval[0] = TIME_ERROR;
		if (time_status & STA_PPSFREQ &&
		    time_status & (STA_PPSWANDER | STA_PPSERROR))
			p->p_retval[0] = TIME_ERROR;
	}
	return error;
}

#ifdef PPS_SYNC

/* We need this ugly monster twice, so let's macroize it. */

#define MEDIAN3X(a, m, s, i1, i2, i3)				\
	do {							\
	m = a[i2];						\
	s = a[i1] - a[i3];					\
	} while (0)

#define MEDIAN3(a, m, s)					\
	do {							\
		if (a[0] > a[1]) {				\
			if (a[1] > a[2])			\
				MEDIAN3X(a, m, s, 0, 1, 2);	\
			else if (a[2] > a[0])			\
				MEDIAN3X(a, m, s, 2, 0, 1);	\
			else					\
				MEDIAN3X(a, m, s, 0, 2, 1);	\
		} else {					\
			if (a[2] > a[1])			\
				MEDIAN3X(a, m, s, 2, 1, 0);	\
			else  if (a[0] > a[2])			\
				MEDIAN3X(a, m, s, 1, 0, 2);	\
			else					\
				MEDIAN3X(a, m, s, 1, 2, 0);	\
		}						\
	} while (0)

/*
 * hardpps() - discipline CPU clock oscillator to external PPS signal
 *
 * This routine is called at each PPS interrupt in order to discipline
 * the CPU clock oscillator to the PPS signal. It measures the PPS phase
 * and leaves it in a handy spot for the hardclock() routine. It
 * integrates successive PPS phase differences and calculates the
 * frequency offset. This is used in hardclock() to discipline the CPU
 * clock oscillator so that intrinsic frequency error is cancelled out.
 * The code requires the caller to capture the time and hardware counter
 * value at the on-time PPS signal transition.
 *
 * Note that, on some Unix systems, this routine runs at an interrupt
 * priority level higher than the timer interrupt routine hardclock().
 * Therefore, the variables used are distinct from the hardclock()
 * variables, except for certain exceptions: The PPS frequency pps_freq
 * and phase pps_offset variables are determined by this routine and
 * updated atomically. The time_tolerance variable can be considered a
 * constant, since it is infrequently changed, and then only when the
 * PPS signal is disabled. The watchdog counter pps_valid is updated
 * once per second by hardclock() and is atomically cleared in this
 * routine.
 */
void
hardpps(tvp, p_usec)
	struct timeval *tvp;		/* time at PPS */
	long p_usec;			/* hardware counter at PPS */
{
	long u_usec, v_usec, bigtick;
	long cal_sec, cal_usec;

	/*
	 * An occasional glitch can be produced when the PPS interrupt
	 * occurs in the hardclock() routine before the time variable is
	 * updated. Here the offset is discarded when the difference
	 * between it and the last one is greater than tick/2, but not
	 * if the interval since the first discard exceeds 30 s.
	 */
	time_status |= STA_PPSSIGNAL;
	time_status &= ~(STA_PPSJITTER | STA_PPSWANDER | STA_PPSERROR);
	pps_valid = 0;
	u_usec = -tvp->tv_usec;
	if (u_usec < -500000)
		u_usec += 1000000;
	v_usec = pps_offset - u_usec;
	if (v_usec < 0)
		v_usec = -v_usec;
	if (v_usec > (tick >> 1)) {
		if (pps_glitch > MAXGLITCH) {
			pps_glitch = 0;
			pps_tf[2] = u_usec;
			pps_tf[1] = u_usec;
		} else {
			pps_glitch++;
			u_usec = pps_offset;
		}
	} else
		pps_glitch = 0;

	/*
	 * A three-stage median filter is used to help deglitch the pps
	 * time. The median sample becomes the time offset estimate; the
	 * difference between the other two samples becomes the time
	 * dispersion (jitter) estimate.
	 */
	pps_tf[2] = pps_tf[1];
	pps_tf[1] = pps_tf[0];
	pps_tf[0] = u_usec;
	MEDIAN3(pps_tf, pps_offset, v_usec);
	if (v_usec > MAXTIME)
		pps_jitcnt++;
	v_usec = (v_usec << PPS_AVG) - pps_jitter;
	if (v_usec < 0)
		pps_jitter -= -v_usec >> PPS_AVG;
	else
		pps_jitter += v_usec >> PPS_AVG;
	if (pps_jitter > (MAXTIME >> 1))
		time_status |= STA_PPSJITTER;

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
	pps_usec -= pps_freq;
	if (pps_usec >= bigtick)
		pps_usec -= bigtick;
	if (pps_usec < 0)
		pps_usec += bigtick;
	pps_time.tv_sec++;
	pps_count++;
	if (pps_count < (1 << pps_shift))
		return;
	pps_count = 0;
	pps_calcnt++;
	u_usec = p_usec << SHIFT_USEC;
	v_usec = pps_usec - u_usec;
	if (v_usec >= bigtick >> 1)
		v_usec -= bigtick;
	if (v_usec < -(bigtick >> 1))
		v_usec += bigtick;
	if (v_usec < 0)
		v_usec = -(-v_usec >> pps_shift);
	else
		v_usec = v_usec >> pps_shift;
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
	    || v_usec > time_tolerance || v_usec < -time_tolerance) {
		pps_errcnt++;
		pps_shift = PPS_SHIFT;
		pps_intcnt = 0;
		time_status |= STA_PPSERROR;
		return;
	}

	/*
	 * A three-stage median filter is used to help deglitch the pps
	 * frequency. The median sample becomes the frequency offset
	 * estimate; the difference between the other two samples
	 * becomes the frequency dispersion (stability) estimate.
	 */
	pps_ff[2] = pps_ff[1];
	pps_ff[1] = pps_ff[0];
	pps_ff[0] = v_usec;
	MEDIAN3(pps_ff, u_usec, v_usec);

	/*
	 * Here the frequency dispersion (stability) is updated. If it
	 * is less than one-fourth the maximum (MAXFREQ), the frequency
	 * offset is updated as well, but clamped to the tolerance. It
	 * will be processed later by the hardclock() routine.
	 */
	v_usec = (v_usec >> 1) - pps_stabil;
	if (v_usec < 0)
		pps_stabil -= -v_usec >> PPS_AVG;
	else
		pps_stabil += v_usec >> PPS_AVG;
	if (pps_stabil > MAXFREQ >> 2) {
		pps_stbcnt++;
		time_status |= STA_PPSWANDER;
		return;
	}
	if (time_status & STA_PPSFREQ) {
		if (u_usec < 0) {
			pps_freq -= -u_usec >> PPS_AVG;
			if (pps_freq < -time_tolerance)
				pps_freq = -time_tolerance;
			u_usec = -u_usec;
		} else {
			pps_freq += u_usec >> PPS_AVG;
			if (pps_freq > time_tolerance)
				pps_freq = time_tolerance;
		}
	}

	/*
	 * Here the calibration interval is adjusted. If the maximum
	 * time difference is greater than tick / 4, reduce the interval
	 * by half. If this is not the case for four consecutive
	 * intervals, double the interval.
	 */
	if (u_usec << pps_shift > bigtick >> 2) {
		pps_intcnt = 0;
		if (pps_shift > PPS_SHIFT)
			pps_shift--;
	} else if (pps_intcnt >= 4) {
		pps_intcnt = 0;
		if (pps_shift < PPS_SHIFTMAX)
			pps_shift++;
	} else
		pps_intcnt++;
}

#endif /* PPS_SYNC */
