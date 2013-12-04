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
#define CLOCK_FLL	.25	/* FLL loop gain */
#define	CLOCK_ALLAN	11	/* Allan intercept (log2 s) */
#define CLOCK_DAY	86400.	/* one day in seconds (s) */
#define CLOCK_JUNE	(CLOCK_DAY * 30) /* June in seconds (s) */
#define CLOCK_LIMIT	30	/* poll-adjust threshold */
#define CLOCK_PGATE	4.	/* poll-adjust gate */
#define PPS_MAXAGE	120	/* kernel pps signal timeout (s) */
#define	FREQTOD(x)	((x) / 65536e6) /* NTP to double */ 
#define	DTOFREQ(x)	((int32)((x) * 65536e6)) /* double to NTP */

/*
 * Clock discipline state machine. This is used to control the
 * synchronization behavior during initialization and following a
 * timewarp.
 *
 *	State	< step		> step		Comments
 *	========================================================
 *	NSET	FREQ		step, FREQ	freq not set
 *
 *	FSET	SYNC		step, SYNC	freq set
 *
 *	FREQ	if (mu < 900)	if (mu < 900)	set freq direct
 *		    ignore	    ignore
 *		else		else
 *		    freq, SYNC	    freq, step, SYNC
 *
 *	SYNC	SYNC		SPIK, ignore	adjust phase/freq
 *
 *	SPIK	SYNC		if (mu < 900)	adjust phase/freq
 *				    ignore
 *				step, SYNC
 */
/*
 * Kernel PLL/PPS state machine. This is used with the kernel PLL
 * modifications described in the documentation.
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
 * Each PPS time/frequency discipline can be enabled by the atom driver
 * or another driver. If enabled, the STA_PPSTIME and STA_FREQ bits are
 * set in the kernel status word; otherwise, these bits are cleared.
 * These bits are also cleard if the kernel reports an error.
 *
 * If an external clock is present, the clock driver sets STA_CLK in the
 * status word. When the local clock driver sees this bit, it updates
 * via this routine, which then calls ntp_adjtime() with the STA_PLL bit
 * set to zero, in which case the system clock is not adjusted. This is
 * also a signal for the external clock driver to discipline the system
 * clock. Unless specified otherwise, all times are in seconds.
 */
/*
 * Program variables that can be tinkered.
 */
double	clock_max = CLOCK_MAX;	/* step threshold */
double	clock_minstep = CLOCK_MINSTEP; /* stepout threshold */
double	clock_panic = CLOCK_PANIC; /* panic threshold */
double	clock_phi = CLOCK_PHI;	/* dispersion rate (s/s) */
u_char	allan_xpt = CLOCK_ALLAN; /* Allan intercept (log2 s) */

/*
 * Program variables
 */
static double clock_offset;	/* offset */
double	clock_jitter;		/* offset jitter */
double	drift_comp;		/* frequency (s/s) */
double	clock_stability;	/* frequency stability (wander) (s/s) */
double	clock_codec;		/* audio codec frequency (samples/s) */
static u_long clock_epoch;	/* last update */
u_int	sys_tai;		/* TAI offset from UTC */
static void rstclock (int, double); /* transition function */
static double direct_freq(double); /* direct set frequency */
static void set_freq(double);	/* set frequency */

#ifdef KERNEL_PLL
static struct timex ntv;	/* ntp_adjtime() parameters */
int	pll_status;		/* last kernel status bits */
#if defined(STA_NANO) && NTP_API == 4
static u_int loop_tai;		/* last TAI offset */
#endif /* STA_NANO */
#endif /* KERNEL_PLL */

/*
 * Clock state machine control flags
 */
int	ntp_enable = 1;		/* clock discipline enabled */
int	pll_control;		/* kernel support available */
int	kern_enable = 1;	/* kernel support enabled */
int	pps_enable;		/* kernel PPS discipline enabled */
int	ext_enable;		/* external clock enabled */
int	pps_stratum;		/* pps stratum */
int	allow_panic = FALSE;	/* allow panic correction */
int	mode_ntpdate = FALSE;	/* exit on first clock set */

/*
 * Clock state machine variables
 */
int	state;			/* clock discipline state */
u_char	sys_poll;		/* time constant/poll (log2 s) */
int	tc_counter;		/* jiggle counter */
double	last_offset;		/* last offset (s) */
static u_long last_step;	/* last clock step */

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
static void pll_trap (int);	/* configuration trap */
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
	 * Initialize state variables.
	 */
	sys_poll = ntp_minpoll;
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
 * sys_rootdisp variable equal to the peer dispersion.
 */
int
local_clock(
	struct	peer *peer,	/* synch source peer structure */
	double	fp_offset	/* clock offset (s) */
	)
{
	int	rval;		/* return code */
	int	osys_poll;	/* old system poll */
	double	mu;		/* interval since last update */
	double	clock_frequency; /* clock frequency */
	double	dtemp, etemp;	/* double temps */
	char	tbuf[80];	/* report buffer */

	/*
	 * If the loop is opened or the NIST LOCKCLOCK is in use,
	 * monitor and record the offsets anyway in order to determine
	 * the open-loop response and then go home.
	 */
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
		snprintf(tbuf, sizeof(tbuf),
		    "%+.0f s; set clock manually within %.0f s.",
		    fp_offset, clock_panic);
		report_event(EVNT_SYSFAULT, NULL, tbuf);
		return (-1);
	}

	/*
	 * This section simulates ntpdate. If the offset exceeds the
	 * step threshold (128 ms), step the clock to that time and
	 * exit. Othewise, slew the clock to that time and exit. Note
	 * that the slew will persist and eventually complete beyond the
	 * life of this program. Note that while ntpdate is active, the
	 * terminal does not detach, so the termination message prints
	 * directly to the terminal.
	 */
	if (mode_ntpdate) {
		if (fabs(fp_offset) > clock_max && clock_max > 0) {
			step_systime(fp_offset);
			msyslog(LOG_NOTICE, "ntpd: time set %+.6f s",
	   		    fp_offset);
			printf("ntpd: time set %+.6fs\n", fp_offset);
		} else {
			adj_systime(fp_offset);
			msyslog(LOG_NOTICE, "ntpd: time slew %+.6f s",
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
	 */
	if (sys_huffpuff != NULL) {
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
	 * Clock state machine transition function which defines how the
	 * system reacts to large phase and frequency excursion. There
	 * are two main regimes: when the offset exceeds the step
	 * threshold (128 ms) and when it does not. Under certain
	 * conditions updates are suspended until the stepout theshold
	 * (900 s) is exceeded. See the documentation on how these
	 * thresholds interact with commands and command line options. 
	 *
	 * Note the kernel is disabled if step is disabled or greater
	 * than 0.5 s or in ntpdate mode. 
	 */
	osys_poll = sys_poll;
	if (sys_poll < peer->minpoll)
		sys_poll = peer->minpoll;
	if (sys_poll > peer->maxpoll)
		sys_poll = peer->maxpoll;
	mu = current_time - clock_epoch;
	clock_frequency = drift_comp;
	rval = 1;
	if (fabs(fp_offset) > clock_max && clock_max > 0) {
		switch (state) {

		/*
		 * In SYNC state we ignore the first outlyer and switch
		 * to SPIK state.
		 */
		case EVNT_SYNC:
			snprintf(tbuf, sizeof(tbuf), "%+.6f s",
			    fp_offset);
			report_event(EVNT_SPIK, NULL, tbuf);
			state = EVNT_SPIK;
			return (0);

		/*
		 * In FREQ state we ignore outlyers and inlyers. At the
		 * first outlyer after the stepout threshold, compute
		 * the apparent frequency correction and step the phase.
		 */
		case EVNT_FREQ:
			if (mu < clock_minstep)
				return (0);

			clock_frequency = direct_freq(fp_offset);

			/* fall through to S_SPIK */

		/*
		 * In SPIK state we ignore succeeding outlyers until
		 * either an inlyer is found or the stepout threshold is
		 * exceeded.
		 */
		case EVNT_SPIK:
			if (mu < clock_minstep)
				return (0);

			/* fall through to default */

		/*
		 * We get here by default in NSET and FSET states and
		 * from above in FREQ or SPIK states.
		 *
		 * In NSET state an initial frequency correction is not
		 * available, usually because the frequency file has not
		 * yet been written. Since the time is outside the step
		 * threshold, the clock is stepped. The frequency will
		 * be set directly following the stepout interval.
		 *
		 * In FSET state the initial frequency has been set from
		 * the frequency file. Since the time is outside the
		 * step threshold, the clock is stepped immediately,
		 * rather than after the stepout interval. Guys get
		 * nervous if it takes 15 minutes to set the clock for
		 * the first time.
		 *
		 * In FREQ and SPIK states the stepout threshold has
		 * expired and the phase is still above the step
		 * threshold. Note that a single spike greater than the
		 * step threshold is always suppressed, even with a
		 * long time constant.
		 */ 
		default:
			snprintf(tbuf, sizeof(tbuf), "%+.6f s",
			    fp_offset);
			report_event(EVNT_CLOCKRESET, NULL, tbuf);
			step_systime(fp_offset);
			reinit_timer();
			tc_counter = 0;
			clock_jitter = LOGTOD(sys_precision);
			rval = 2;
			if (state == EVNT_NSET || (current_time -
			    last_step) < clock_minstep * 2) {
				rstclock(EVNT_FREQ, 0);
				return (rval);
			}
			last_step = current_time;
			break;
		}
		rstclock(EVNT_SYNC, 0);
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
		 * In NSET state this is the first update received and
		 * the frequency has not been initialized. Adjust the
		 * phase, but do not adjust the frequency until after
		 * the stepout threshold.
		 */
		case EVNT_NSET:
			rstclock(EVNT_FREQ, fp_offset);
			break;

		/*
		 * In FSET state this is the first update received and
		 * the frequency has been initialized. Adjust the phase,
		 * but do not adjust the frequency until the next
		 * update.
		 */
		case EVNT_FSET:
			rstclock(EVNT_SYNC, fp_offset);
			break;

		/*
		 * In FREQ state ignore updates until the stepout
		 * threshold. After that, compute the new frequency, but
		 * do not adjust the phase or frequency until the next
		 * update.
		 */
		case EVNT_FREQ:
			if (mu < clock_minstep)
				return (0);

			clock_frequency = direct_freq(fp_offset);
			rstclock(EVNT_SYNC, 0);
			break;


		/*
		 * We get here by default in SYNC and SPIK states. Here
		 * we compute the frequency update due to PLL and FLL
		 * contributions.
		 */
		default:
			allow_panic = FALSE;

			/*
			 * The FLL and PLL frequency gain constants
			 * depend on the time constant and Allan
			 * intercept. The PLL is always used, but
			 * becomes ineffective above the Allan intercept
			 * where the FLL becomes effective.
			 */
			if (sys_poll >= allan_xpt)
				clock_frequency += (fp_offset -
				    clock_offset) /
				    max(ULOGTOD(sys_poll), mu) *
				    CLOCK_FLL;

			/*
			 * The PLL frequency gain (numerator) depends on
			 * the minimum of the update interval and Allan
			 * intercept. This reduces the PLL gain when the 
			 * FLL becomes effective.
			 */ 
			etemp = min(ULOGTOD(allan_xpt), mu);
			dtemp = 4 * CLOCK_PLL * ULOGTOD(sys_poll);
			clock_frequency += fp_offset * etemp / (dtemp *
			    dtemp);
			rstclock(EVNT_SYNC, fp_offset);
			break;
		}
	}

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
			ntv.esterror = (u_int32)(clock_jitter * 1e6);
			ntv.maxerror = (u_int32)((sys_rootdelay / 2 +
			    sys_rootdisp) * 1e6);
			ntv.status = STA_PLL;

			/*
			 * Enable/disable the PPS if requested.
			 */
			if (pps_enable) {
				if (!(pll_status & STA_PPSTIME))
					report_event(EVNT_KERN,
					    NULL, "PPS enabled");
				ntv.status |= STA_PPSTIME | STA_PPSFREQ;
			} else {
				if (pll_status & STA_PPSTIME)
					report_event(EVNT_KERN,
					    NULL, "PPS disabled");
				ntv.status &= ~(STA_PPSTIME |
				    STA_PPSFREQ);
			}
			if (sys_leap == LEAP_ADDSECOND)
				ntv.status |= STA_INS;
			else if (sys_leap == LEAP_DELSECOND)
				ntv.status |= STA_DEL;
		}

		/*
		 * Pass the stuff to the kernel. If it squeals, turn off
		 * the pps. In any case, fetch the kernel offset,
		 * frequency and jitter.
		 */
		if (ntp_adjtime(&ntv) == TIME_ERROR) {
			if (!(ntv.status & STA_PPSSIGNAL))
				report_event(EVNT_KERN, NULL,
				    "PPS no signal");
		}
		pll_status = ntv.status;
#ifdef STA_NANO
		clock_offset = ntv.offset / 1e9;
#else /* STA_NANO */
		clock_offset = ntv.offset / 1e6;
#endif /* STA_NANO */
		clock_frequency = FREQTOD(ntv.freq);

		/*
		 * If the kernel PPS is lit, monitor its performance.
		 */
		if (ntv.status & STA_PPSTIME) {
#ifdef STA_NANO
			clock_jitter = ntv.jitter / 1e9;
#else /* STA_NANO */
			clock_jitter = ntv.jitter / 1e6;
#endif /* STA_NANO */
		}

#if defined(STA_NANO) && NTP_API == 4
		/*
		 * If the TAI changes, update the kernel TAI.
		 */
		if (loop_tai != sys_tai) {
			loop_tai = sys_tai;
			ntv.modes = MOD_TAI;
			ntv.constant = sys_tai;
			ntp_adjtime(&ntv);
		}
#endif /* STA_NANO */
	}
#endif /* KERNEL_PLL */

	/*
	 * Clamp the frequency within the tolerance range and calculate
	 * the frequency difference since the last update.
	 */
	if (fabs(clock_frequency) > NTP_MAXFREQ)
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
	 * Calculate the wander as the exponentially weighted RMS
	 * frequency differences. Record the change for the frequency
	 * file update.
	 */
	etemp = SQUARE(clock_stability);
	clock_stability = SQRT(etemp + (dtemp - etemp) / CLOCK_AVG);
	drift_file_sw = TRUE;

	/*
	 * Here we adjust the timeconstan by comparing the current
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
	 * If the time constant has changed, update the poll variables.
	 */
	if (osys_poll != sys_poll)
		poll_update(peer, sys_poll);

	/*
	 * Yibbidy, yibbbidy, yibbidy; that'h all folks.
	 */
	record_loop_stats(clock_offset, drift_comp, clock_jitter,
	    clock_stability, sys_poll);
#ifdef DEBUG
	if (debug)
		printf(
		    "local_clock: offset %.9f jit %.9f freq %.3f stab %.3f poll %d\n",
		    clock_offset, clock_jitter, drift_comp * 1e6,
		    clock_stability * 1e6, sys_poll);
#endif /* DEBUG */
	return (rval);
#endif /* LOCKCLOCK */
}


/*
 * adj_host_clock - Called once every second to update the local clock.
 *
 * LOCKCLOCK: The only thing this routine does is increment the
 * sys_rootdisp variable.
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
	 * would be counterproductive.
	 */
	sys_rootdisp += clock_phi;

#ifndef LOCKCLOCK
	/*
	 * If clock discipline is disabled or if the kernel is enabled,
	 * get out of Dodge quick.
	 */
	if (!ntp_enable || mode_ntpdate || (pll_control &&
	    kern_enable))
		return;

	/*
	 * Implement the phase and frequency adjustments. The gain
	 * factor (denominator) increases with poll interval, so is
	 * dominated by the FLL above the Allan intercept.
 	 */
	adjustment = clock_offset / (CLOCK_PLL * ULOGTOD(sys_poll));
	clock_offset -= adjustment;
	adj_systime(adjustment + drift_comp);
#endif /* LOCKCLOCK */
}


/*
 * Clock state machine. Enter new state and set state variables.
 */
static void
rstclock(
	int	trans,		/* new state */
	double	offset		/* new offset */
	)
{
#ifdef DEBUG
	if (debug > 1)
		printf("local_clock: mu %lu state %d poll %d count %d\n",
		    current_time - clock_epoch, trans, sys_poll,
		    tc_counter);
#endif
	if (trans != state && trans != EVNT_FSET)
		report_event(trans, NULL, NULL);
	state = trans;
	last_offset = clock_offset = offset;
	clock_epoch = current_time; 
}

/*
 * calc_freq - calculate frequency directly
 *
 * This is very carefully done. When the offset is first computed at the
 * first update, a residual frequency component results. Subsequently,
 * updates are suppresed until the end of the measurement interval while
 * the offset is amortized. At the end of the interval the frequency is
 * calculated from the current offset, residual offset, length of the
 * interval and residual frequency component. At the same time the
 * frequenchy file is armed for update at the next hourly stats.
 */
static double
direct_freq(
	double	fp_offset
	)
{

#ifdef KERNEL_PLL
	/*
	 * If the kernel is enabled, we need the residual offset to
	 * calculate the frequency correction.
	 */
	if (pll_control && kern_enable) {
		memset(&ntv,  0, sizeof(ntv));
		ntp_adjtime(&ntv);
#ifdef STA_NANO
		clock_offset = ntv.offset / 1e9;
#else /* STA_NANO */
		clock_offset = ntv.offset / 1e6;
#endif /* STA_NANO */
		drift_comp = FREQTOD(ntv.freq);
	}
#endif /* KERNEL_PLL */
	set_freq((fp_offset - clock_offset) / (current_time -
	    clock_epoch) + drift_comp);
	wander_resid = 0;
	return (drift_comp);
}


/*
 * set_freq - set clock frequency
 */
static void
set_freq(
	double	freq		/* frequency update */
	)
{
	char	tbuf[80];

	drift_comp = freq;

#ifdef KERNEL_PLL
	/*
	 * If the kernel is enabled, update the kernel frequency.
	 */
	if (pll_control && kern_enable) {
		memset(&ntv,  0, sizeof(ntv));
		ntv.modes = MOD_FREQUENCY;
		ntv.freq = DTOFREQ(drift_comp);
		ntp_adjtime(&ntv);
		snprintf(tbuf, sizeof(tbuf), "kernel %.3f PPM",
		    drift_comp * 1e6);
		report_event(EVNT_FSET, NULL, tbuf);
	} else {
		snprintf(tbuf, sizeof(tbuf), "ntpd %.3f PPM",
		    drift_comp * 1e6);
		report_event(EVNT_FSET, NULL, tbuf);
	}
#else /* KERNEL_PLL */
	snprintf(tbuf, sizeof(tbuf), "ntpd %.3f PPM", drift_comp *
	    1e6);
	report_event(EVNT_FSET, NULL, tbuf);
#endif /* KERNEL_PLL */
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
	int	item,
	double	freq
	)
{
	int i;

#ifdef DEBUG
	if (debug > 1)
		printf("loop_config: item %d freq %f\n", item, freq);
#endif
	switch (item) {

	/*
	 * We first assume the kernel supports the ntp_adjtime()
	 * syscall. If that syscall works, initialize the kernel time
	 * variables. Otherwise, continue leaving no harm behind.
	 */
	case LOOP_DRIFTINIT:
#ifndef LOCKCLOCK
#ifdef KERNEL_PLL
		if (mode_ntpdate)
			break;

		pll_control = 1;
		memset(&ntv, 0, sizeof(ntv));
		ntv.modes = MOD_BITS;
		ntv.status = STA_PLL;
		ntv.maxerror = MAXDISPERSE;
		ntv.esterror = MAXDISPERSE;
		ntv.constant = sys_poll;
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
			report_event(EVNT_KERN, NULL,
 		  	    "kernel time sync enabled");
		}
#endif /* KERNEL_PLL */
#endif /* LOCKCLOCK */
		break;

	/*
	 * Initialize the frequency. If the frequency file is missing or
	 * broken, set the initial frequency to zero and set the state
	 * to NSET. Otherwise, set the initial frequency to the given
	 * value and the state to FSET.
	 */
	case LOOP_DRIFTCOMP:
#ifndef LOCKCLOCK
		if (freq > NTP_MAXFREQ || freq < -NTP_MAXFREQ) {
			set_freq(0);
			rstclock(EVNT_NSET, 0);
		} else {
			set_freq(freq);
			rstclock(EVNT_FSET, 0);
		}
#endif /* LOCKCLOCK */
		break;

	/*
	 * Disable the kernel at shutdown. The microkernel just abandons
	 * ship. The nanokernel carefully cleans up so applications can
	 * see this. Note the last programmed offset and frequency are
	 * left in place.
	 */
	case LOOP_KERN_CLEAR:
#ifndef LOCKCLOCK
#ifdef KERNEL_PLL
		if (pll_control && kern_enable) {
			memset((char *)&ntv, 0, sizeof(ntv));
			ntv.modes = MOD_STATUS;
			ntv.status = STA_UNSYNC;
			ntp_adjtime(&ntv);
			report_event(EVNT_KERN, NULL,
 		  	    "kernel time sync disabledx");
		   }
#endif /* KERNEL_PLL */
#endif /* LOCKCLOCK */
		break;

	/*
	 * Tinker command variables for Ulrich Windl. Very dangerous.
	 */
	case LOOP_ALLAN:	/* Allan intercept (log2) (allan) */
		allan_xpt = (u_char)freq;
		break;

	case LOOP_CODEC:	/* audio codec frequency (codec) */
		clock_codec = freq / 1e6;
		break;
	
	case LOOP_PHI:		/* dispersion threshold (dispersion) */
		clock_phi = freq / 1e6;
		break;

	case LOOP_FREQ:		/* initial frequency (freq) */	
		set_freq(freq / 1e6);
		rstclock(EVNT_FSET, 0);
		break;

	case LOOP_HUFFPUFF:	/* huff-n'-puff length (huffpuff) */
		if (freq < HUFFPUFF)
			freq = HUFFPUFF;
		sys_hufflen = (int)(freq / HUFFPUFF);
		sys_huffpuff = (double *)emalloc(sizeof(double) *
		    sys_hufflen);
		for (i = 0; i < sys_hufflen; i++)
			sys_huffpuff[i] = 1e9;
		sys_mindly = 1e9;
		break;

	case LOOP_PANIC:	/* panic threshold (panic) */
		clock_panic = freq;
		break;

	case LOOP_MAX:		/* step threshold (step) */
		clock_max = freq;
		if (clock_max == 0 || clock_max > 0.5)
			kern_enable = 0;
		break;

	case LOOP_MINSTEP:	/* stepout threshold (stepout) */
		clock_minstep = freq; 
		break;

	case LOOP_LEAP:		/* not used */
	default:
		msyslog(LOG_NOTICE,
		    "loop_config: unsupported option %d", item);
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
