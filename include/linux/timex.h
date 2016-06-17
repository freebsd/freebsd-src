/*****************************************************************************
 *                                                                           *
 * Copyright (c) David L. Mills 1993                                         *
 *                                                                           *
 * Permission to use, copy, modify, and distribute this software and its     *
 * documentation for any purpose and without fee is hereby granted, provided *
 * that the above copyright notice appears in all copies and that both the   *
 * copyright notice and this permission notice appear in supporting          *
 * documentation, and that the name University of Delaware not be used in    *
 * advertising or publicity pertaining to distribution of the software       *
 * without specific, written prior permission.  The University of Delaware   *
 * makes no representations about the suitability this software for any      *
 * purpose.  It is provided "as is" without express or implied warranty.     *
 *                                                                           *
 *****************************************************************************/

/*
 * Modification history timex.h
 *
 * 29 Dec 97	Russell King
 *	Moved CLOCK_TICK_RATE, CLOCK_TICK_FACTOR and FINETUNE to asm/timex.h
 *	for ARM machines
 *
 *  9 Jan 97    Adrian Sun
 *      Shifted LATCH define to allow access to alpha machines.
 *
 * 26 Sep 94	David L. Mills
 *	Added defines for hybrid phase/frequency-lock loop.
 *
 * 19 Mar 94	David L. Mills
 *	Moved defines from kernel routines to header file and added new
 *	defines for PPS phase-lock loop.
 *
 * 20 Feb 94	David L. Mills
 *	Revised status codes and structures for external clock and PPS
 *	signal discipline.
 *
 * 28 Nov 93	David L. Mills
 *	Adjusted parameters to improve stability and increase poll
 *	interval.
 *
 * 17 Sep 93    David L. Mills
 *      Created file $NTP/include/sys/timex.h
 * 07 Oct 93    Torsten Duwe
 *      Derived linux/timex.h
 * 1995-08-13    Torsten Duwe
 *      kernel PLL updated to 1994-12-13 specs (rfc-1589)
 * 1997-08-30    Ulrich Windl
 *      Added new constant NTP_PHASE_LIMIT
 */
#ifndef _LINUX_TIMEX_H
#define _LINUX_TIMEX_H

#include <asm/param.h>

/*
 * The following defines establish the engineering parameters of the PLL
 * model. The HZ variable establishes the timer interrupt frequency, 100 Hz
 * for the SunOS kernel, 256 Hz for the Ultrix kernel and 1024 Hz for the
 * OSF/1 kernel. The SHIFT_HZ define expresses the same value as the
 * nearest power of two in order to avoid hardware multiply operations.
 */
#if HZ >= 12 && HZ < 24
# define SHIFT_HZ	4
#elif HZ >= 24 && HZ < 48
# define SHIFT_HZ	5
#elif HZ >= 48 && HZ < 96
# define SHIFT_HZ	6
#elif HZ >= 96 && HZ < 192
# define SHIFT_HZ	7
#elif HZ >= 192 && HZ < 384
# define SHIFT_HZ	8
#elif HZ >= 384 && HZ < 768
# define SHIFT_HZ	9
#elif HZ >= 768 && HZ < 1536
# define SHIFT_HZ	10
#else
# error You lose.
#endif

/*
 * SHIFT_KG and SHIFT_KF establish the damping of the PLL and are chosen
 * for a slightly underdamped convergence characteristic. SHIFT_KH
 * establishes the damping of the FLL and is chosen by wisdom and black
 * art.
 *
 * MAXTC establishes the maximum time constant of the PLL. With the
 * SHIFT_KG and SHIFT_KF values given and a time constant range from
 * zero to MAXTC, the PLL will converge in 15 minutes to 16 hours,
 * respectively.
 */
#define SHIFT_KG 6		/* phase factor (shift) */
#define SHIFT_KF 16		/* PLL frequency factor (shift) */
#define SHIFT_KH 2		/* FLL frequency factor (shift) */
#define MAXTC 6			/* maximum time constant (shift) */

/*
 * The SHIFT_SCALE define establishes the decimal point of the time_phase
 * variable which serves as an extension to the low-order bits of the
 * system clock variable. The SHIFT_UPDATE define establishes the decimal
 * point of the time_offset variable which represents the current offset
 * with respect to standard time. The FINEUSEC define represents 1 usec in
 * scaled units.
 *
 * SHIFT_USEC defines the scaling (shift) of the time_freq and
 * time_tolerance variables, which represent the current frequency
 * offset and maximum frequency tolerance.
 *
 * FINEUSEC is 1 us in SHIFT_UPDATE units of the time_phase variable.
 */
#define SHIFT_SCALE 22		/* phase scale (shift) */
#define SHIFT_UPDATE (SHIFT_KG + MAXTC) /* time offset scale (shift) */
#define SHIFT_USEC 16		/* frequency offset scale (shift) */
#define FINEUSEC (1L << SHIFT_SCALE) /* 1 us in phase units */

#define MAXPHASE 512000L        /* max phase error (us) */
#define MAXFREQ (512L << SHIFT_USEC)  /* max frequency error (ppm) */
#define MAXTIME (200L << PPS_AVG) /* max PPS error (jitter) (200 us) */
#define MINSEC 16L              /* min interval between updates (s) */
#define MAXSEC 1200L            /* max interval between updates (s) */
#define	NTP_PHASE_LIMIT	(MAXPHASE << 5)	/* beyond max. dispersion */

/*
 * The following defines are used only if a pulse-per-second (PPS)
 * signal is available and connected via a modem control lead, such as
 * produced by the optional ppsclock feature incorporated in the Sun
 * asynch driver. They establish the design parameters of the frequency-
 * lock loop used to discipline the CPU clock oscillator to the PPS
 * signal.
 *
 * PPS_AVG is the averaging factor for the frequency loop, as well as
 * the time and frequency dispersion.
 *
 * PPS_SHIFT and PPS_SHIFTMAX specify the minimum and maximum
 * calibration intervals, respectively, in seconds as a power of two.
 *
 * PPS_VALID is the maximum interval before the PPS signal is considered
 * invalid and protocol updates used directly instead.
 *
 * MAXGLITCH is the maximum interval before a time offset of more than
 * MAXTIME is believed.
 */
#define PPS_AVG 2		/* pps averaging constant (shift) */
#define PPS_SHIFT 2		/* min interval duration (s) (shift) */
#define PPS_SHIFTMAX 8		/* max interval duration (s) (shift) */
#define PPS_VALID 120		/* pps signal watchdog max (s) */
#define MAXGLITCH 30		/* pps signal glitch max (s) */

/*
 * Pick up the architecture specific timex specifications
 */
#include <asm/timex.h>

/* LATCH is used in the interval timer and ftape setup. */
#define LATCH  ((CLOCK_TICK_RATE + HZ/2) / HZ)	/* For divider */

/*
 * syscall interface - used (mainly by NTP daemon)
 * to discipline kernel clock oscillator
 */
struct timex {
	unsigned int modes;	/* mode selector */
	long offset;		/* time offset (usec) */
	long freq;		/* frequency offset (scaled ppm) */
	long maxerror;		/* maximum error (usec) */
	long esterror;		/* estimated error (usec) */
	int status;		/* clock command/status */
	long constant;		/* pll time constant */
	long precision;		/* clock precision (usec) (read only) */
	long tolerance;		/* clock frequency tolerance (ppm)
				 * (read only)
				 */
	struct timeval time;	/* (read only) */
	long tick;		/* (modified) usecs between clock ticks */

	long ppsfreq;           /* pps frequency (scaled ppm) (ro) */
	long jitter;            /* pps jitter (us) (ro) */
	int shift;              /* interval duration (s) (shift) (ro) */
	long stabil;            /* pps stability (scaled ppm) (ro) */
	long jitcnt;            /* jitter limit exceeded (ro) */
	long calcnt;            /* calibration intervals (ro) */
	long errcnt;            /* calibration errors (ro) */
	long stbcnt;            /* stability limit exceeded (ro) */

	int  :32; int  :32; int  :32; int  :32;
	int  :32; int  :32; int  :32; int  :32;
	int  :32; int  :32; int  :32; int  :32;
};

/*
 * Mode codes (timex.mode)
 */
#define ADJ_OFFSET		0x0001	/* time offset */
#define ADJ_FREQUENCY		0x0002	/* frequency offset */
#define ADJ_MAXERROR		0x0004	/* maximum time error */
#define ADJ_ESTERROR		0x0008	/* estimated time error */
#define ADJ_STATUS		0x0010	/* clock status */
#define ADJ_TIMECONST		0x0020	/* pll time constant */
#define ADJ_TICK		0x4000	/* tick value */
#define ADJ_OFFSET_SINGLESHOT	0x8001	/* old-fashioned adjtime */

/* xntp 3.4 compatibility names */
#define MOD_OFFSET	ADJ_OFFSET
#define MOD_FREQUENCY	ADJ_FREQUENCY
#define MOD_MAXERROR	ADJ_MAXERROR
#define MOD_ESTERROR	ADJ_ESTERROR
#define MOD_STATUS	ADJ_STATUS
#define MOD_TIMECONST	ADJ_TIMECONST
#define MOD_CLKB	ADJ_TICK
#define MOD_CLKA	ADJ_OFFSET_SINGLESHOT /* 0x8000 in original */


/*
 * Status codes (timex.status)
 */
#define STA_PLL		0x0001	/* enable PLL updates (rw) */
#define STA_PPSFREQ	0x0002	/* enable PPS freq discipline (rw) */
#define STA_PPSTIME	0x0004	/* enable PPS time discipline (rw) */
#define STA_FLL		0x0008	/* select frequency-lock mode (rw) */

#define STA_INS		0x0010	/* insert leap (rw) */
#define STA_DEL		0x0020	/* delete leap (rw) */
#define STA_UNSYNC	0x0040	/* clock unsynchronized (rw) */
#define STA_FREQHOLD	0x0080	/* hold frequency (rw) */

#define STA_PPSSIGNAL	0x0100	/* PPS signal present (ro) */
#define STA_PPSJITTER	0x0200	/* PPS signal jitter exceeded (ro) */
#define STA_PPSWANDER	0x0400	/* PPS signal wander exceeded (ro) */
#define STA_PPSERROR	0x0800	/* PPS signal calibration error (ro) */

#define STA_CLOCKERR	0x1000	/* clock hardware fault (ro) */

#define STA_RONLY (STA_PPSSIGNAL | STA_PPSJITTER | STA_PPSWANDER | \
    STA_PPSERROR | STA_CLOCKERR) /* read-only bits */

/*
 * Clock states (time_state)
 */
#define TIME_OK		0	/* clock synchronized, no leap second */
#define TIME_INS	1	/* insert leap second */
#define TIME_DEL	2	/* delete leap second */
#define TIME_OOP	3	/* leap second in progress */
#define TIME_WAIT	4	/* leap second has occurred */
#define TIME_ERROR	5	/* clock not synchronized */
#define TIME_BAD	TIME_ERROR /* bw compat */

#ifdef __KERNEL__
/*
 * kernel variables
 * Note: maximum error = NTP synch distance = dispersion + delay / 2;
 * estimated error = NTP dispersion.
 */
extern long tick;                      /* timer interrupt period */
extern int tickadj;			/* amount of adjustment per tick */

/*
 * phase-lock loop variables
 */
extern int time_state;		/* clock status */
extern int time_status;		/* clock synchronization status bits */
extern long time_offset;	/* time adjustment (us) */
extern long time_constant;	/* pll time constant */
extern long time_tolerance;	/* frequency tolerance (ppm) */
extern long time_precision;	/* clock precision (us) */
extern long time_maxerror;	/* maximum error */
extern long time_esterror;	/* estimated error */

extern long time_phase;		/* phase offset (scaled us) */
extern long time_freq;		/* frequency offset (scaled ppm) */
extern long time_adj;		/* tick adjust (scaled 1 / HZ) */
extern long time_reftime;	/* time at last adjustment (s) */

extern long time_adjust;	/* The amount of adjtime left */

/* interface variables pps->timer interrupt */
extern long pps_offset;		/* pps time offset (us) */
extern long pps_jitter;		/* time dispersion (jitter) (us) */
extern long pps_freq;		/* frequency offset (scaled ppm) */
extern long pps_stabil;		/* frequency dispersion (scaled ppm) */
extern long pps_valid;		/* pps signal watchdog counter */

/* interface variables pps->adjtimex */
extern int pps_shift;		/* interval duration (s) (shift) */
extern long pps_jitcnt;		/* jitter limit exceeded */
extern long pps_calcnt;		/* calibration intervals */
extern long pps_errcnt;		/* calibration errors */
extern long pps_stbcnt;		/* stability limit exceeded */

#endif /* KERNEL */

#endif /* LINUX_TIMEX_H */
