/*
 * ntp_loopfilter.c - implements the NTP loop filter algorithm
 *
 * ATTENTION: Get approval from Dave Mills on all changes to this file!
 *
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <ctype.h>

#include <signal.h>
#include <setjmp.h>

#if defined(VMS) && defined(VMS_LOCALUNIT)	/*wjm*/
#include "ntp_refclock.h"
#endif /* VMS */

#ifdef KERNEL_PLL
#include "ntp_syscall.h"
#endif /* KERNEL_PLL */

/*
 * This is an implementation of the clock discipline algorithm described
 * in UDel TR 97-4-3, as amended. It operates as an adaptive parameter,
 * hybrid phase/frequency-lock loop. A number of sanity checks are
 * included to protect against timewarps, timespikes and general mayhem.
 * All units are in s and s/s, unless noted otherwise.
 */
#define CLOCK_MAX	.128	/* default step threshold (s) */
#define CLOCK_MINSTEP	900.	/* default stepout threshold (s) */
#define CLOCK_PANIC	1000.	/* default panic threshold (s) */
#define	CLOCK_PHI	15e-6	/* max frequency error (s/s) */
#define CLOCK_PLL	16.	/* PLL loop gain (log2) */
#define CLOCK_AVG	8.	/* parameter averaging constant */
#define CLOCK_FLL	(NTP_MAXPOLL + CLOCK_AVG) /* FLL loop gain */
#define	CLOCK_ALLAN	1500.	/* compromise Allan intercept (s) */
#define CLOCK_DAY	86400.	/* one day in seconds (s) */
#define CLOCK_JUNE	(CLOCK_DAY * 30) /* June in seconds (s) */
#define CLOCK_LIMIT	30	/* poll-adjust threshold */
#define CLOCK_PGATE	4.	/* poll-adjust gate */
#define PPS_MAXAGE	120	/* kernel pps signal timeout (s) */

/*
 * Clock discipline state machine. This is used to control the
 * synchronization behavior during initialization and following a
 * timewarp.
 *
 *	State	< step		> step		Comments
 *	====================================================
 *	NSET	FREQ		step, FREQ	no ntp.drift
 *
 *	FSET	SYNC		step, SYNC	ntp.drift
 *
 *	FREQ	if (mu < 900)	if (mu < 900)	set freq
 *		    ignore	    ignore
 *		else		else
 *		    freq, SYNC	    freq, step, SYNC
 *
 *	SYNC	SYNC		if (mu < 900)	adjust phase/freq
 *				    ignore
 *				else
 *				    SPIK
 *
 *	SPIK	SYNC		step, SYNC	set phase
 */
#define S_NSET	0		/* clock never set */
#define S_FSET	1		/* frequency set from the drift file */
#define S_SPIK	2		/* spike detected */
#define S_FREQ	3		/* frequency mode */
#define S_SYNC	4		/* clock synchronized */

/*
 * Kernel PLL/PPS state machine. This is used with the kernel PLL
 * modifications described in the README.kernel file.
 *
 * If kernel support for the ntp_adjtime() system call is available, the
 * ntp_control flag is set. The ntp_enable and kern_enable flags can be
 * set at configuration time or run time using ntpdc. If ntp_enable is
 * false, the discipline loop is unlocked and no corrections of any kind
 * are made. If both ntp_control and kern_enable are set, the kernel
 * support is used as described above; if false, the kernel is bypassed
 * entirely and the daemon discipline used instead.
 *
 * There have been three versions of the kernel discipline code. The
 * first (microkernel) now in Solaris discipilnes the microseconds. The
 * second and third (nanokernel) disciplines the clock in nanoseconds.
 * These versions are identifed if the symbol STA_PLL is present in the
 * header file /usr/include/sys/timex.h. The third and current version
 * includes TAI offset and is identified by the symbol NTP_API with
 * value 4.
 *
 * Each update to a prefer peer sets pps_stratum if it survives the
 * intersection algorithm and its time is within range. The PPS time
 * discipline is enabled (STA_PPSTIME bit set in the status word) when
 * pps_stratum is true and the PPS frequency discipline is enabled. If
 * the PPS time discipline is enabled and the kernel reports a PPS
 * signal is present, the pps_control variable is set to the current
 * time. If the current time is later than pps_control by PPS_MAXAGE
 * (120 s), this variable is set to zero.
 *
 * If an external clock is present, the clock driver sets STA_CLK in the
 * status word. When the local clock driver sees this bit, it updates
 * via this routine, which then calls ntp_adjtime() with the STA_PLL bit
 * set to zero, in which case the system clock is not adjusted. This is
 * also a signal for the external clock driver to discipline the system
 * clock.
 */
/*
 * Program variables that can be tinkered.
 */
double	clock_max = CLOCK_MAX;	/* step threshold (s) */
double	clock_minstep = CLOCK_MINSTEP; /* stepout threshold (s) */
double	clock_panic = CLOCK_PANIC; /* panic threshold (s) */
double	clock_phi = CLOCK_PHI;	/* dispersion rate (s/s) */
double	allan_xpt = CLOCK_ALLAN; /* Allan intercept (s) */

/*
 * Program variables
 */
static double clock_offset;	/* offset (s) */
double	clock_jitter;		/* offset jitter (s) */
double	drift_comp;		/* frequency (s/s) */
double	clock_stability;	/* frequency stability (wander) (s/s) */
u_long	sys_clocktime;		/* last system clock update */
u_long	pps_control;		/* last pps update */
u_long	sys_tai;		/* UTC offset from TAI (s) */
static void rstclock P((int, u_long, double)); /* transition function */

#ifdef KERNEL_PLL
struct timex ntv;		/* kernel API parameters */
int	pll_status;		/* status bits for kernel pll */
#endif /* KERNEL_PLL */

/*
 * Clock state machine control flags
 */
int	ntp_enable;		/* clock discipline enabled */
int	pll_control;		/* kernel support available */
int	kern_enable;		/* kernel support enabled */
int	pps_enable;		/* kernel PPS discipline enabled */
int	ext_enable;		/* external clock enabled */
int	pps_stratum;		/* pps stratum */
int	allow_panic = FALSE;	/* allow panic correction */
int	mode_ntpdate = FALSE;	/* exit on first clock set */

/*
 * Clock state machine variables
 */
int	state;			/* clock discipline state */
u_char	sys_poll = NTP_MINDPOLL; /* time constant/poll (log2 s) */
int	tc_counter;		/* jiggle counter */
double	last_offset;		/* last offset (s) */

/*
 * Huff-n'-puff filter variables
 */
static double *sys_huffpuff;	/* huff-n'-puff filter */
static int sys_hufflen;		/* huff-n'-puff filter stages */
static int sys_huffptr;		/* huff-n'-puff filter pointer */
static double sys_mindly;	/* huff-n'-puff filter min delay */

#if defined(KERNEL_PLL)
/* Emacs cc-mode goes nuts if we split the next line... */
#define MOD_BITS (MOD_OFFSET | MOD_MAXERROR | MOD_ESTERROR | \
    MOD_STATUS | MOD_TIMECONST)
#ifdef SIGSYS
static void pll_trap P((int));	/* configuration trap */
static struct sigaction sigsys;	/* current sigaction status */
static struct sigaction newsigsys; /* new sigaction status */
static sigjmp_buf env;		/* environment var. for pll_trap() */
#endif /* SIGSYS */
#endif /* KERNEL_PLL */

/*
 * init_loopfilter - initialize loop filter data
 */
void
init_loopfilter(void)
{
	/*
	 * Initialize state variables. Initially, we expect no drift
	 * file, so set the state to S_NSET. If a drift file is present,
	 * it will be detected later and the state set to S_FSET.
	 */
	rstclock(S_NSET, 0, 0);
	clock_jitter = LOGTOD(sys_precision);
}

/*
 * local_clock - the NTP logical clock loop filter.
 *
 * Return codes:
 * -1	update ignored: exceeds panic threshold
 * 0	update ignored: popcorn or exceeds step threshold
 * 1	clock was slewed
 * 2	clock was stepped
 *
 * LOCKCLOCK: The only thing this routine does is set the
 * sys_rootdispersion variable equal to the peer dispersion.
 */
int
local_clock(
	struct	peer *peer,	/* synch source peer structure */
	double	fp_offset	/* clock offset (s) */
	)
{
	int	rval;		/* return code */
	u_long	mu;		/* interval since last update (s) */
	double	flladj;		/* FLL frequency adjustment (ppm) */
	double	plladj;		/* PLL frequency adjustment (ppm) */
	double	clock_frequency; /* clock frequency adjustment (ppm) */
	double	dtemp, etemp;	/* double temps */
#ifdef OPENSSL
	u_int32 *tpt;
	int	i;
	u_int	len;
	long	togo;
#endif /* OPENSSL */

	/*
	 * If the loop is opened or the NIST LOCKCLOCK is in use,
	 * monitor and record the offsets anyway in order to determine
	 * the open-loop response and then go home.
	 */
#ifdef DEBUG
	if (debug)
		printf(
		    "local_clock: assocID %d offset %.9f freq %.3f state %d\n",
		    peer->associd, fp_offset, drift_comp * 1e6, state);
#endif
#ifdef LOCKCLOCK
	return (0);

#else /* LOCKCLOCK */
	if (!ntp_enable) {
		record_loop_stats(fp_offset, drift_comp, clock_jitter,
		    clock_stability, sys_poll);
		return (0);
	}

	/*
	 * If the clock is way off, panic is declared. The clock_panic
	 * defaults to 1000 s; if set to zero, the panic will never
	 * occur. The allow_panic defaults to FALSE, so the first panic
	 * will exit. It can be set TRUE by a command line option, in
	 * which case the clock will be set anyway and time marches on.
	 * But, allow_panic will be set FALSE when the update is less
	 * than the step threshold; so, subsequent panics will exit.
	 */
	if (fabs(fp_offset) > clock_panic && clock_panic > 0 &&
	    !allow_panic) {
		msyslog(LOG_ERR,
		    "time correction of %.0f seconds exceeds sanity limit (%.0f); set clock manually to the correct UTC time.",
		    fp_offset, clock_panic);
		return (-1);
	}

	/*
	 * If simulating ntpdate, set the clock directly, rather than
	 * using the discipline. The clock_max defines the step
	 * threshold, above which the clock will be stepped instead of
	 * slewed. The value defaults to 128 ms, but can be set to even
	 * unreasonable values. If set to zero, the clock will never be
	 * stepped. Note that a slew will persist beyond the life of
	 * this program.
	 *
	 * Note that if ntpdate is active, the terminal does not detach,
	 * so the termination comments print directly to the console.
	 */
	if (mode_ntpdate) {
		if (fabs(fp_offset) > clock_max && clock_max > 0) {
			step_systime(fp_offset);
			msyslog(LOG_NOTICE, "time reset %+.6f s",
	   		    fp_offset);
			printf("ntpd: time set %+.6fs\n", fp_offset);
		} else {
			adj_systime(fp_offset);
			msyslog(LOG_NOTICE, "time slew %+.6f s",
			    fp_offset);
			printf("ntpd: time slew %+.6fs\n", fp_offset);
		}
		record_loop_stats(fp_offset, drift_comp, clock_jitter,
		    clock_stability, sys_poll);
		exit (0);
	}

	/*
	 * The huff-n'-puff filter finds the lowest delay in the recent
	 * interval. This is used to correct the offset by one-half the
	 * difference between the sample delay and minimum delay. This
	 * is most effective if the delays are highly assymetric and
	 * clockhopping is avoided and the clock frequency wander is
	 * relatively small.
	 *
	 * Note either there is no prefer peer or this update is from
	 * the prefer peer.
	 */
	if (sys_huffpuff != NULL && (sys_prefer == NULL || sys_prefer ==
	    peer)) {
		if (peer->delay < sys_huffpuff[sys_huffptr])
			sys_huffpuff[sys_huffptr] = peer->delay;
		if (peer->delay < sys_mindly)
			sys_mindly = peer->delay;
		if (fp_offset > 0)
			dtemp = -(peer->delay - sys_mindly) / 2;
		else
			dtemp = (peer->delay - sys_mindly) / 2;
		fp_offset += dtemp;
#ifdef DEBUG
		if (debug)
			printf(
		    "local_clock: size %d mindly %.6f huffpuff %.6f\n",
			    sys_hufflen, sys_mindly, dtemp);
#endif
	}

	/*
	 * Clock state machine transition function. This is where the
	 * action is and defines how the system reacts to large phase
	 * and frequency errors. There are two main regimes: when the
	 * offset exceeds the step threshold and when it does not.
	 * However, if the step threshold is set to zero, a step will
	 * never occur. See the instruction manual for the details how
	 * these actions interact with the command line options.
	 *
	 * Note the system poll is set to minpoll only if the clock is
	 * stepped. Note also the kernel is disabled if step is
	 * disabled or greater than 0.5 s. 
	 */
	clock_frequency = flladj = plladj = 0;
	mu = peer->epoch - sys_clocktime;
	if (clock_max == 0 || clock_max > 0.5)
		kern_enable = 0;
	rval = 1;
	if (fabs(fp_offset) > clock_max && clock_max > 0) {
		switch (state) {

		/*
		 * In S_SYNC state we ignore the first outlyer amd
		 * switch to S_SPIK state.
		 */
		case S_SYNC:
			state = S_SPIK;
			return (0);

		/*
		 * In S_FREQ state we ignore outlyers and inlyers. At
		 * the first outlyer after the stepout threshold,
		 * compute the apparent frequency correction and step
		 * the phase.
		 */
		case S_FREQ:
			if (mu < clock_minstep)
				return (0);

			clock_frequency = (fp_offset - clock_offset) /
			    mu;

			/* fall through to S_SPIK */

		/*
		 * In S_SPIK state we ignore succeeding outlyers until
		 * either an inlyer is found or the stepout threshold is
		 * exceeded.
		 */
		case S_SPIK:
			if (mu < clock_minstep)
				return (0);

			/* fall through to default */

		/*
		 * We get here by default in S_NSET and S_FSET states
		 * and from above in S_FREQ or S_SPIK states.
		 *
		 * In S_NSET state an initial frequency correction is
		 * not available, usually because the frequency file has
		 * not yet been written. Since the time is outside the
		 * step threshold, the clock is stepped. The frequency
		 * will be set directly following the stepout interval.
		 *
		 * In S_FSET state the initial frequency has been set
		 * from the frequency file. Since the time is outside
		 * the step threshold, the clock is stepped immediately,
		 * rather than after the stepout interval. Guys get
		 * nervous if it takes 17 minutes to set the clock for
		 * the first time.
		 *
		 * In S_FREQ and S_SPIK states the stepout threshold has
		 * expired and the phase is still above the step
		 * threshold. Note that a single spike greater than the
		 * step threshold is always suppressed, even at the
		 * longer poll intervals.
		 */ 
		default:
			step_systime(fp_offset);
			msyslog(LOG_NOTICE, "time reset %+.6f s",
			    fp_offset);
			reinit_timer();
			tc_counter = 0;
			sys_poll = NTP_MINPOLL;
			sys_tai = 0;
			clock_jitter = LOGTOD(sys_precision);
			rval = 2;
			if (state == S_NSET) {
				rstclock(S_FREQ, peer->epoch, 0);
				return (rval);
			}
			break;
		}
		rstclock(S_SYNC, peer->epoch, 0);
	} else {

		/*
		 * The offset is less than the step threshold. Calculate
		 * the jitter as the exponentially weighted offset
		 * differences.
 	      	 */
		etemp = SQUARE(clock_jitter);
		dtemp = SQUARE(max(fabs(fp_offset - last_offset),
		    LOGTOD(sys_precision)));
		clock_jitter = SQRT(etemp + (dtemp - etemp) /
		    CLOCK_AVG);
		switch (state) {

		/*
		 * In S_NSET state this is the first update received and
		 * the frequency has not been initialized. Adjust the
		 * phase, but do not adjust the frequency until after
		 * the stepout threshold.
		 */
		case S_NSET:
			rstclock(S_FREQ, peer->epoch, fp_offset);
			break;

		/*
		 * In S_FSET state this is the first update received and
		 * the frequency has been initialized. Adjust the phase,
		 * but do not adjust the frequency until the next
		 * update.
		 */
		case S_FSET:
			rstclock(S_SYNC, peer->epoch, fp_offset);
			break;

		/*
		 * In S_FREQ state ignore updates until the stepout
		 * threshold. After that, correct the phase and
		 * frequency and switch to S_SYNC state.
		 */
		case S_FREQ:
			if (mu < clock_minstep)
				return (0);

			clock_frequency = (fp_offset - clock_offset) /
			    mu;
			rstclock(S_SYNC, peer->epoch, fp_offset);
			break;

		/*
		 * We get here by default in S_SYNC and S_SPIK states.
		 * Here we compute the frequency update due to PLL and
		 * FLL contributions.
		 */
		default:
			allow_panic = FALSE;

			/*
			 * The FLL and PLL frequency gain constants
			 * depend on the poll interval and Allan
			 * intercept. The PLL is always used, but
			 * becomes ineffective above the Allan
			 * intercept. The FLL is not used below one-half
			 * the Allan intercept. Above that the loop gain
			 * increases in steps to 1 / CLOCK_AVG. 
			 */
			if (ULOGTOD(sys_poll) > allan_xpt / 2) {
				dtemp = CLOCK_FLL - sys_poll;
				flladj = (fp_offset - clock_offset) /
				    (max(mu, allan_xpt) * dtemp);
			}

			/*
			 * For the PLL the integration interval
			 * (numerator) is the minimum of the update
			 * interval and poll interval. This allows
			 * oversampling, but not undersampling.
			 */ 
			etemp = min(mu, (u_long)ULOGTOD(sys_poll));
			dtemp = 4 * CLOCK_PLL * ULOGTOD(sys_poll);
			plladj = fp_offset * etemp / (dtemp * dtemp);
			rstclock(S_SYNC, peer->epoch, fp_offset);
			break;
		}
	}

#ifdef OPENSSL
	/*
	 * Scan the loopsecond table to determine the TAI offset. If
	 * there is a scheduled leap in future, set the leap warning,
	 * but only if less than 30 days before the leap.
	 */
	tpt = (u_int32 *)tai_leap.ptr;
	len = ntohl(tai_leap.vallen) / sizeof(u_int32);
	if (tpt != NULL) {
		for (i = 0; i < len; i++) {
			togo = (long)ntohl(tpt[i]) -
			    (long)peer->rec.l_ui;
			if (togo > 0) {
				if (togo < CLOCK_JUNE)
					leap_next |= LEAP_ADDSECOND;
				break;
			}
		}
#if defined(STA_NANO) && NTP_API == 4
		if (pll_control && kern_enable && sys_tai == 0) {
			memset(&ntv, 0, sizeof(ntv));
			ntv.modes = MOD_TAI;
			ntv.constant = i + TAI_1972 - 1;
			ntp_adjtime(&ntv);
		}
#endif /* STA_NANO */
		sys_tai = i + TAI_1972 - 1;
	}
#endif /* OPENSSL */
#ifdef KERNEL_PLL
	/*
	 * This code segment works when clock adjustments are made using
	 * precision time kernel support and the ntp_adjtime() system
	 * call. This support is available in Solaris 2.6 and later,
	 * Digital Unix 4.0 and later, FreeBSD, Linux and specially
	 * modified kernels for HP-UX 9 and Ultrix 4. In the case of the
	 * DECstation 5000/240 and Alpha AXP, additional kernel
	 * modifications provide a true microsecond clock and nanosecond
	 * clock, respectively.
	 *
	 * Important note: The kernel discipline is used only if the
	 * step threshold is less than 0.5 s, as anything higher can
	 * lead to overflow problems. This might occur if some misguided
	 * lad set the step threshold to something ridiculous.
	 */
	if (pll_control && kern_enable) {

		/*
		 * We initialize the structure for the ntp_adjtime()
		 * system call. We have to convert everything to
		 * microseconds or nanoseconds first. Do not update the
		 * system variables if the ext_enable flag is set. In
		 * this case, the external clock driver will update the
		 * variables, which will be read later by the local
		 * clock driver. Afterwards, remember the time and
		 * frequency offsets for jitter and stability values and
		 * to update the frequency file.
		 */
		memset(&ntv,  0, sizeof(ntv));
		if (ext_enable) {
			ntv.modes = MOD_STATUS;
		} else {
			struct tm *tm = NULL;
			time_t tstamp;

#ifdef STA_NANO
			ntv.modes = MOD_BITS | MOD_NANO;
#else /* STA_NANO */
			ntv.modes = MOD_BITS;
#endif /* STA_NANO */
			if (clock_offset < 0)
				dtemp = -.5;
			else
				dtemp = .5;
#ifdef STA_NANO
			ntv.offset = (int32)(clock_offset * 1e9 +
			    dtemp);
			ntv.constant = sys_poll;
#else /* STA_NANO */
			ntv.offset = (int32)(clock_offset * 1e6 +
			    dtemp);
			ntv.constant = sys_poll - 4;
#endif /* STA_NANO */

			/*
			 * The frequency is set directly only if
			 * clock_frequency is nonzero coming out of FREQ
			 * state.
			 */
			if (clock_frequency != 0) {
				ntv.modes |= MOD_FREQUENCY;
				ntv.freq = (int32)((clock_frequency +
				    drift_comp) * 65536e6);
			}
			ntv.esterror = (u_int32)(clock_jitter * 1e6);
			ntv.maxerror = (u_int32)((sys_rootdelay / 2 +
			    sys_rootdispersion) * 1e6);
			ntv.status = STA_PLL;

			/*
			 * Set the leap bits in the status word, but
			 * only on the last day of June or December.
			 */
			tstamp = peer->rec.l_ui - JAN_1970;
			tm = gmtime(&tstamp);
			if (tm != NULL) {
				if ((tm->tm_mon + 1 == 6 &&
				    tm->tm_mday == 30) || (tm->tm_mon +
				    1 == 12 && tm->tm_mday == 31)) {
					if (leap_next & LEAP_ADDSECOND)
						ntv.status |= STA_INS;
					else if (leap_next &
					    LEAP_DELSECOND)
						ntv.status |= STA_DEL;
				}
			}

			/*
			 * If the PPS signal is up and enabled, light
			 * the frequency bit. If the PPS driver is
			 * working, light the phase bit as well. If not,
			 * douse the lights, since somebody else may
			 * have left the switch on.
			 */
			if (pps_enable && pll_status & STA_PPSSIGNAL) {
				ntv.status |= STA_PPSFREQ;
				if (pps_stratum < STRATUM_UNSPEC)
					ntv.status |= STA_PPSTIME;
			} else {
				ntv.status &= ~(STA_PPSFREQ |
				    STA_PPSTIME);
			}
		}

		/*
		 * Pass the stuff to the kernel. If it squeals, turn off
		 * the pig. In any case, fetch the kernel offset and
		 * frequency and pretend we did it here.
		 */
		if (ntp_adjtime(&ntv) == TIME_ERROR) {
			NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
			    msyslog(LOG_NOTICE,
			    "kernel time sync error %04x", ntv.status);
			ntv.status &= ~(STA_PPSFREQ | STA_PPSTIME);
		}
		pll_status = ntv.status;
#ifdef STA_NANO
		clock_offset = ntv.offset / 1e9;
#else /* STA_NANO */
		clock_offset = ntv.offset / 1e6;
#endif /* STA_NANO */
		clock_frequency = ntv.freq / 65536e6;
		flladj = plladj = 0;

		/*
		 * If the kernel PPS is lit, monitor its performance.
		 */
		if (ntv.status & STA_PPSTIME) {
			pps_control = current_time;
#ifdef STA_NANO
			clock_jitter = ntv.jitter / 1e9;
#else /* STA_NANO */
			clock_jitter = ntv.jitter / 1e6;
#endif /* STA_NANO */
		}
	} else {
#endif /* KERNEL_PLL */
 
		/*
		 * We get here if the kernel discipline is not enabled.
		 * Adjust the clock frequency as the sum of the directly
		 * computed frequency (if measured) and the PLL and FLL
		 * increments.
		 */
		clock_frequency = drift_comp + clock_frequency +
		    flladj + plladj;
#ifdef KERNEL_PLL
	}
#endif /* KERNEL_PLL */

	/*
	 * Clamp the frequency within the tolerance range and calculate
	 * the frequency change since the last update.
	 */
	if (fabs(clock_frequency) > NTP_MAXFREQ)
		NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
		    msyslog(LOG_NOTICE,
		    "frequency error %.0f PPM exceeds tolerance %.0f PPM",
		    clock_frequency * 1e6, NTP_MAXFREQ * 1e6);
	dtemp = SQUARE(clock_frequency - drift_comp);
	if (clock_frequency > NTP_MAXFREQ)
		drift_comp = NTP_MAXFREQ;
	else if (clock_frequency < -NTP_MAXFREQ)
		drift_comp = -NTP_MAXFREQ;
	else
		drift_comp = clock_frequency;

	/*
	 * Calculate the wander as the exponentially weighted frequency
	 * differences.
	 */
	etemp = SQUARE(clock_stability);
	clock_stability = SQRT(etemp + (dtemp - etemp) / CLOCK_AVG);

	/*
	 * Here we adjust the poll interval by comparing the current
	 * offset with the clock jitter. If the offset is less than the
	 * clock jitter times a constant, then the averaging interval is
	 * increased, otherwise it is decreased. A bit of hysteresis
	 * helps calm the dance. Works best using burst mode.
	 */
	if (fabs(clock_offset) < CLOCK_PGATE * clock_jitter) {
		tc_counter += sys_poll;
		if (tc_counter > CLOCK_LIMIT) {
			tc_counter = CLOCK_LIMIT;
			if (sys_poll < peer->maxpoll) {
				tc_counter = 0;
				sys_poll++;
			}
		}
	} else {
		tc_counter -= sys_poll << 1;
		if (tc_counter < -CLOCK_LIMIT) {
			tc_counter = -CLOCK_LIMIT;
			if (sys_poll > peer->minpoll) {
				tc_counter = 0;
				sys_poll--;
			}
		}
	}

	/*
	 * Yibbidy, yibbbidy, yibbidy; that'h all folks.
	 */
	record_loop_stats(clock_offset, drift_comp, clock_jitter,
	    clock_stability, sys_poll);
#ifdef DEBUG
	if (debug)
		printf(
		    "local_clock: mu %lu jitr %.6f freq %.3f stab %.6f poll %d count %d\n",
		    mu, clock_jitter, drift_comp * 1e6,
		    clock_stability * 1e6, sys_poll, tc_counter);
#endif /* DEBUG */
	return (rval);
#endif /* LOCKCLOCK */
}


/*
 * adj_host_clock - Called once every second to update the local clock.
 *
 * LOCKCLOCK: The only thing this routine does is increment the
 * sys_rootdispersion variable.
 */
void
adj_host_clock(
	void
	)
{
	double	adjustment;

	/*
	 * Update the dispersion since the last update. In contrast to
	 * NTPv3, NTPv4 does not declare unsynchronized after one day,
	 * since the dispersion check serves this function. Also,
	 * since the poll interval can exceed one day, the old test
	 * would be counterproductive. Note we do this even with
	 * external clocks, since the clock driver will recompute the
	 * maximum error and the local clock driver will pick it up and
	 * pass to the common refclock routines. Very elegant.
	 */
	sys_rootdispersion += clock_phi;

#ifndef LOCKCLOCK
	/*
	 * If clock discipline is disabled or if the kernel is enabled,
	 * get out of Dodge quick.
	 */
	if (!ntp_enable || mode_ntpdate || (pll_control &&
	    kern_enable))
		return;

	/*
	 * Declare PPS kernel unsync if the pps signal has not been
	 * heard for a few minutes.
	 */
	if (pps_control && current_time - pps_control > PPS_MAXAGE) {
		if (pps_control)
			NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
			    msyslog(LOG_NOTICE, "pps sync disabled");
		pps_control = 0;
	}

	/*
	 * Implement the phase and frequency adjustments. The gain
	 * factor (denominator) is not allowed to increase beyond the
	 * Allan intercept. It doesn't make sense to average phase noise
	 * beyond this point and it helps to damp residual offset at the
	 * longer poll intervals.
	 */
	adjustment = clock_offset / (CLOCK_PLL * min(ULOGTOD(sys_poll),
	    allan_xpt));
	clock_offset -= adjustment;
	adj_systime(adjustment + drift_comp);
#endif /* LOCKCLOCK */
}


/*
 * Clock state machine. Enter new state and set state variables. Note we
 * use the time of the last clock filter sample, which may be earlier
 * than the current time.
 */
static void
rstclock(
	int	trans,		/* new state */
	u_long	update,		/* new update time */
	double	offset		/* new offset */
	)
{
#ifdef DEBUG
	if (debug)
		printf("local_clock: time %lu offset %.6f freq %.3f state %d\n",
		    update, offset, drift_comp * 1e6, trans);
#endif
	state = trans;
	sys_clocktime = update;
	last_offset = clock_offset = offset;
}


/*
 * huff-n'-puff filter
 */
void
huffpuff()
{
	int i;

	if (sys_huffpuff == NULL)
		return;

	sys_huffptr = (sys_huffptr + 1) % sys_hufflen;
	sys_huffpuff[sys_huffptr] = 1e9;
	sys_mindly = 1e9;
	for (i = 0; i < sys_hufflen; i++) {
		if (sys_huffpuff[i] < sys_mindly)
			sys_mindly = sys_huffpuff[i];
	}
}


/*
 * loop_config - configure the loop filter
 *
 * LOCKCLOCK: The LOOP_DRIFTINIT and LOOP_DRIFTCOMP cases are no-ops.
 */
void
loop_config(
	int item,
	double freq
	)
{
	int i;

	switch (item) {

	case LOOP_DRIFTINIT:

#ifndef LOCKCLOCK
#ifdef KERNEL_PLL
		/*
		 * Assume the kernel supports the ntp_adjtime() syscall.
		 * If that syscall works, initialize the kernel time
 		 * variables. Otherwise, continue leaving no harm
		 * behind. While at it, ask to set nanosecond mode. If
		 * the kernel agrees, rejoice; othewise, it does only
		 * microseconds.
		 */
		if (mode_ntpdate)
			break;

		pll_control = 1;
		memset(&ntv, 0, sizeof(ntv));
#ifdef STA_NANO
		ntv.modes = MOD_BITS | MOD_NANO;
#else /* STA_NANO */
		ntv.modes = MOD_BITS;
#endif /* STA_NANO */
		ntv.maxerror = MAXDISPERSE;
		ntv.esterror = MAXDISPERSE;
		ntv.status = STA_UNSYNC;
#ifdef SIGSYS
		/*
		 * Use sigsetjmp() to save state and then call
		 * ntp_adjtime(); if it fails, then siglongjmp() is used
		 * to return control
		 */
		newsigsys.sa_handler = pll_trap;
		newsigsys.sa_flags = 0;
		if (sigaction(SIGSYS, &newsigsys, &sigsys)) {
			msyslog(LOG_ERR,
			    "sigaction() fails to save SIGSYS trap: %m");
			pll_control = 0;
		}
		if (sigsetjmp(env, 1) == 0)
			ntp_adjtime(&ntv);
		if ((sigaction(SIGSYS, &sigsys,
		    (struct sigaction *)NULL))) {
			msyslog(LOG_ERR,
			    "sigaction() fails to restore SIGSYS trap: %m");
			pll_control = 0;
		}
#else /* SIGSYS */
		ntp_adjtime(&ntv);
#endif /* SIGSYS */

		/*
		 * Save the result status and light up an external clock
		 * if available.
		 */
		pll_status = ntv.status;
		if (pll_control) {
#ifdef STA_NANO
			if (pll_status & STA_CLK)
				ext_enable = 1;
#endif /* STA_NANO */
			NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
			    msyslog(LOG_INFO,
		  	    "kernel time sync status %04x",
			    pll_status);
		}
#endif /* KERNEL_PLL */
#endif /* LOCKCLOCK */
		break;

	case LOOP_DRIFTCOMP:

#ifndef LOCKCLOCK
		/*
		 * If the frequency value is reasonable, set the initial
		 * frequency to the given value and the state to S_FSET.
		 * Otherwise, the drift file may be missing or broken,
		 * so set the frequency to zero. This erases past
		 * history should somebody break something.
		 */
		if (freq <= NTP_MAXFREQ && freq >= -NTP_MAXFREQ) {
			drift_comp = freq;
			rstclock(S_FSET, 0, 0);
		} else {
			drift_comp = 0;
		}

#ifdef KERNEL_PLL
		/*
		 * Sanity check. If the kernel is available, load the
		 * frequency and light up the loop. Make sure the offset
		 * is zero to cancel any previous nonsense. If you don't
		 * want this initialization, remove the ntp.drift file.
		 */
		if (pll_control && kern_enable) {
			memset((char *)&ntv, 0, sizeof(ntv));
			ntv.modes = MOD_OFFSET | MOD_FREQUENCY;
			ntv.freq = (int32)(drift_comp * 65536e6);
			ntp_adjtime(&ntv);
		}
#endif /* KERNEL_PLL */
#endif /* LOCKCLOCK */
		break;

	case LOOP_KERN_CLEAR:
#ifndef LOCKCLOCK
#ifdef KERNEL_PLL
		/* Completely turn off the kernel time adjustments. */
		if (pll_control) {
			memset((char *)&ntv, 0, sizeof(ntv));
			ntv.modes = MOD_BITS | MOD_OFFSET | MOD_FREQUENCY;
			ntv.status = STA_UNSYNC;
			ntp_adjtime(&ntv);
			NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
			    msyslog(LOG_INFO,
		  	    "kernel time sync disabled %04x",
			    ntv.status);
		   }
#endif /* KERNEL_PLL */
#endif /* LOCKCLOCK */
		break;

	/*
	 * Special tinker variables for Ulrich Windl. Very dangerous.
	 */
	case LOOP_MAX:			/* step threshold */
		clock_max = freq;
		break;

	case LOOP_PANIC:		/* panic threshold */
		clock_panic = freq;
		break;

	case LOOP_PHI:			/* dispersion rate */
		clock_phi = freq;
		break;

	case LOOP_MINSTEP:		/* watchdog bark */
		clock_minstep = freq; 
		break;

	case LOOP_ALLAN:		/* Allan intercept */
		allan_xpt = freq;
		break;
	
	case LOOP_HUFFPUFF:		/* huff-n'-puff filter length */
		if (freq < HUFFPUFF)
			freq = HUFFPUFF;
		sys_hufflen = (int)(freq / HUFFPUFF);
		sys_huffpuff = (double *)emalloc(sizeof(double) *
		    sys_hufflen);
		for (i = 0; i < sys_hufflen; i++)
			sys_huffpuff[i] = 1e9;
		sys_mindly = 1e9;
		break;

	case LOOP_FREQ:			/* initial frequency */	
		drift_comp = freq / 1e6;
		rstclock(S_FSET, 0, 0);
		break;
	}
}


#if defined(KERNEL_PLL) && defined(SIGSYS)
/*
 * _trap - trap processor for undefined syscalls
 *
 * This nugget is called by the kernel when the SYS_ntp_adjtime()
 * syscall bombs because the silly thing has not been implemented in
 * the kernel. In this case the phase-lock loop is emulated by
 * the stock adjtime() syscall and a lot of indelicate abuse.
 */
static RETSIGTYPE
pll_trap(
	int arg
	)
{
	pll_control = 0;
	siglongjmp(env, 1);
}
#endif /* KERNEL_PLL && SIGSYS */
