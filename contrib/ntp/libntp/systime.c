/*
 * systime -- routines to fiddle a UNIX clock.
 *
 * ATTENTION: Get approval from Dave Mills on all changes to this file!
 *
 */
#include <config.h>

#include "ntp.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"
#include "ntp_random.h"
#include "iosignal.h"
#include "timevalops.h"
#include "timespecops.h"
#include "ntp_calendar.h"

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_UTMP_H
# include <utmp.h>
#endif /* HAVE_UTMP_H */
#ifdef HAVE_UTMPX_H
# include <utmpx.h>
#endif /* HAVE_UTMPX_H */

int	allow_panic = FALSE;		/* allow panic correction (-g) */
int	enable_panic_check = TRUE;	/* Can we check allow_panic's state? */

#ifndef USE_COMPILETIME_PIVOT
# define USE_COMPILETIME_PIVOT 1
#endif

/*
 * These routines (get_systime, step_systime, adj_systime) implement an
 * interface between the system independent NTP clock and the Unix
 * system clock in various architectures and operating systems. Time is
 * a precious quantity in these routines and every effort is made to
 * minimize errors by unbiased rounding and amortizing adjustment
 * residues.
 *
 * In order to improve the apparent resolution, provide unbiased
 * rounding and most importantly ensure that the readings cannot be
 * predicted, the low-order unused portion of the time below the minimum
 * time to read the clock is filled with an unbiased random fuzz.
 *
 * The sys_tick variable specifies the system clock tick interval in
 * seconds, for stepping clocks, defined as those which return times
 * less than MINSTEP greater than the previous reading. For systems that
 * use a high-resolution counter such that each clock reading is always
 * at least MINSTEP greater than the prior, sys_tick is the time to read
 * the system clock.
 *
 * The sys_fuzz variable measures the minimum time to read the system
 * clock, regardless of its precision.  When reading the system clock
 * using get_systime() after sys_tick and sys_fuzz have been determined,
 * ntpd ensures each unprocessed clock reading is no less than sys_fuzz
 * later than the prior unprocessed reading, and then fuzzes the bits
 * below sys_fuzz in the timestamp returned, ensuring each of its
 * resulting readings is strictly later than the previous.
 *
 * When slewing the system clock using adj_systime() (with the kernel
 * loop discipline unavailable or disabled), adjtime() offsets are
 * quantized to sys_tick, if sys_tick is greater than sys_fuzz, which
 * is to say if the OS presents a stepping clock.  Otherwise, offsets
 * are quantized to the microsecond resolution of adjtime()'s timeval
 * input.  The remaining correction sys_residual is carried into the
 * next adjtime() and meanwhile is also factored into get_systime()
 * readings.
 */
double	sys_tick = 0;		/* tick size or time to read (s) */
double	sys_fuzz = 0;		/* min. time to read the clock (s) */
long	sys_fuzz_nsec = 0;	/* min. time to read the clock (ns) */
double	measured_tick;		/* non-overridable sys_tick (s) */
double	sys_residual = 0;	/* adjustment residue (s) */
int	trunc_os_clock;		/* sys_tick > measured_tick */
time_stepped_callback	step_callback;

#ifndef SIM
/* perlinger@ntp.org: As 'get_sysime()' does it's own check for clock
 * backstepping, this could probably become a local variable in
 * 'get_systime()' and the cruft associated with communicating via a
 * static value could be removed after the v4.2.8 release.
 */
static int lamport_violated;	/* clock was stepped back */
#endif	/* !SIM */

#ifdef DEBUG
static int systime_init_done;
# define DONE_SYSTIME_INIT()	systime_init_done = TRUE
#else
# define DONE_SYSTIME_INIT()	do {} while (FALSE)
#endif

#ifdef HAVE_SIGNALED_IO
int using_sigio;
#endif

#ifdef SYS_WINNT
CRITICAL_SECTION get_systime_cs;
#endif


void
set_sys_fuzz(
	double	fuzz_val
	)
{
	sys_fuzz = fuzz_val;
	INSIST(sys_fuzz >= 0);
	INSIST(sys_fuzz <= 1.0);
	sys_fuzz_nsec = (long)(sys_fuzz * 1e9 + 0.5);
}


void
init_systime(void)
{
	INIT_GET_SYSTIME_CRITSEC();
	INIT_WIN_PRECISE_TIME();
	DONE_SYSTIME_INIT();
}


#ifndef SIM	/* ntpsim.c has get_systime() and friends for sim */

static inline void
get_ostime(
	struct timespec *	tsp
	)
{
	int	rc;
	long	ticks;

#if defined(HAVE_CLOCK_GETTIME)
	rc = clock_gettime(CLOCK_REALTIME, tsp);
#elif defined(HAVE_GETCLOCK)
	rc = getclock(TIMEOFDAY, tsp);
#else
	struct timeval		tv;

	rc = GETTIMEOFDAY(&tv, NULL);
	tsp->tv_sec = tv.tv_sec;
	tsp->tv_nsec = tv.tv_usec * 1000;
#endif
	if (rc < 0) {
		msyslog(LOG_ERR, "read system clock failed: %m (%d)",
			errno);
		exit(1);
	}

	if (trunc_os_clock) {
		ticks = (long)((tsp->tv_nsec * 1e-9) / sys_tick);
		tsp->tv_nsec = (long)(ticks * 1e9 * sys_tick);
	}
}


/*
 * get_systime - return system time in NTP timestamp format.
 */
void
get_systime(
	l_fp *now		/* system time */
	)
{
        static struct timespec  ts_last;        /* last sampled os time */
	static struct timespec	ts_prev;	/* prior os time */
	static l_fp		lfp_prev;	/* prior result */
	static double		dfuzz_prev;	/* prior fuzz */
	struct timespec ts;	/* seconds and nanoseconds */
	struct timespec ts_min;	/* earliest permissible */
	struct timespec ts_lam;	/* lamport fictional increment */
	struct timespec ts_prev_log;	/* for msyslog only */
	double	dfuzz;
	double	ddelta;
	l_fp	result;
	l_fp	lfpfuzz;
	l_fp	lfpdelta;

	get_ostime(&ts);
	DEBUG_REQUIRE(systime_init_done);
	ENTER_GET_SYSTIME_CRITSEC();

        /* First check if here was a Lamport violation, that is, two
         * successive calls to 'get_ostime()' resulted in negative
         * time difference. Use a few milliseconds of permissible
         * tolerance -- being too sharp can hurt here. (This is intented
         * for the Win32 target, where the HPC interpolation might
         * introduce small steps backward. It should not be an issue on
         * systems where get_ostime() results in a true syscall.)
         */
        if (cmp_tspec(add_tspec_ns(ts, 50000000), ts_last) < 0)
                lamport_violated = 1;
        ts_last = ts;

	/*
	 * After default_get_precision() has set a nonzero sys_fuzz,
	 * ensure every reading of the OS clock advances by at least
	 * sys_fuzz over the prior reading, thereby assuring each
	 * fuzzed result is strictly later than the prior.  Limit the
	 * necessary fiction to 1 second.
	 */
	if (!USING_SIGIO()) {
		ts_min = add_tspec_ns(ts_prev, sys_fuzz_nsec);
		if (cmp_tspec(ts, ts_min) < 0) {
			ts_lam = sub_tspec(ts_min, ts);
			if (ts_lam.tv_sec > 0 && !lamport_violated) {
				msyslog(LOG_ERR,
					"get_systime Lamport advance exceeds one second (%.9f)",
					ts_lam.tv_sec +
					    1e-9 * ts_lam.tv_nsec);
				exit(1);
			}
			if (!lamport_violated)
				ts = ts_min;
		}
		ts_prev_log = ts_prev;
		ts_prev = ts;
	} else {
		/*
		 * Quiet "ts_prev_log.tv_sec may be used uninitialized"
		 * warning from x86 gcc 4.5.2.
		 */
		ZERO(ts_prev_log);
	}

	/* convert from timespec to l_fp fixed-point */
	result = tspec_stamp_to_lfp(ts);

	/*
	 * Add in the fuzz.
	 */
	dfuzz = ntp_random() * 2. / FRAC * sys_fuzz;
	DTOLFP(dfuzz, &lfpfuzz);
	L_ADD(&result, &lfpfuzz);

	/*
	 * Ensure result is strictly greater than prior result (ignoring
	 * sys_residual's effect for now) once sys_fuzz has been
	 * determined.
	 */
	if (!USING_SIGIO()) {
		if (!L_ISZERO(&lfp_prev) && !lamport_violated) {
			if (!L_ISGTU(&result, &lfp_prev) &&
			    sys_fuzz > 0.) {
				msyslog(LOG_ERR, "ts_prev %s ts_min %s",
					tspectoa(ts_prev_log),
					tspectoa(ts_min));
				msyslog(LOG_ERR, "ts %s", tspectoa(ts));
				msyslog(LOG_ERR, "sys_fuzz %ld nsec, prior fuzz %.9f",
					sys_fuzz_nsec, dfuzz_prev);
				msyslog(LOG_ERR, "this fuzz %.9f",
					dfuzz);
				lfpdelta = lfp_prev;
				L_SUB(&lfpdelta, &result);
				LFPTOD(&lfpdelta, ddelta);
				msyslog(LOG_ERR,
					"prev get_systime 0x%x.%08x is %.9f later than 0x%x.%08x",
					lfp_prev.l_ui, lfp_prev.l_uf,
					ddelta, result.l_ui, result.l_uf);
			}
		}
		lfp_prev = result;
		dfuzz_prev = dfuzz;
		if (lamport_violated) 
			lamport_violated = FALSE;
	}
	LEAVE_GET_SYSTIME_CRITSEC();
	*now = result;
}


/*
 * adj_systime - adjust system time by the argument.
 */
#if !defined SYS_WINNT
int				/* 0 okay, 1 error */
adj_systime(
	double now		/* adjustment (s) */
	)
{
	struct timeval adjtv;	/* new adjustment */
	struct timeval oadjtv;	/* residual adjustment */
	double	quant;		/* quantize to multiples of */
	double	dtemp;
	long	ticks;
	int	isneg = 0;

	/*
	 * The Windows port adj_systime() depends on being called each
	 * second even when there's no additional correction, to allow
	 * emulation of adjtime() behavior on top of an API that simply
	 * sets the current rate.  This POSIX implementation needs to
	 * ignore invocations with zero correction, otherwise ongoing
	 * EVNT_NSET adjtime() can be aborted by a tiny adjtime()
	 * triggered by sys_residual.
	 */
	if (0. == now) {
		if (enable_panic_check && allow_panic) {
			msyslog(LOG_ERR, "adj_systime: allow_panic is TRUE!");
			INSIST(!allow_panic);
		}
		return TRUE;
	}

	/*
	 * Most Unix adjtime() implementations adjust the system clock
	 * in microsecond quanta, but some adjust in 10-ms quanta. We
	 * carefully round the adjustment to the nearest quantum, then
	 * adjust in quanta and keep the residue for later.
	 */
	dtemp = now + sys_residual;
	if (dtemp < 0) {
		isneg = 1;
		dtemp = -dtemp;
	}
	adjtv.tv_sec = (long)dtemp;
	dtemp -= adjtv.tv_sec;
	if (sys_tick > sys_fuzz)
		quant = sys_tick;
	else
		quant = 1e-6;
	ticks = (long)(dtemp / quant + .5);
	adjtv.tv_usec = (long)(ticks * quant * 1.e6 + .5);
	/* The rounding in the conversions could us push over the
	 * limits: make sure the result is properly normalised!
	 * note: sign comes later, all numbers non-negative here.
	 */
	if (adjtv.tv_usec >= 1000000) {
		adjtv.tv_sec  += 1;
		adjtv.tv_usec -= 1000000;
		dtemp         -= 1.;
	}
	/* set the new residual with leftover from correction */
	sys_residual = dtemp - adjtv.tv_usec * 1.e-6;

	/*
	 * Convert to signed seconds and microseconds for the Unix
	 * adjtime() system call. Note we purposely lose the adjtime()
	 * leftover.
	 */
	if (isneg) {
		adjtv.tv_sec = -adjtv.tv_sec;
		adjtv.tv_usec = -adjtv.tv_usec;
		sys_residual = -sys_residual;
	}
	if (adjtv.tv_sec != 0 || adjtv.tv_usec != 0) {
		if (adjtime(&adjtv, &oadjtv) < 0) {
			msyslog(LOG_ERR, "adj_systime: %m");
			if (enable_panic_check && allow_panic) {
				msyslog(LOG_ERR, "adj_systime: allow_panic is TRUE!");
			}
			return FALSE;
		}
	}
	if (enable_panic_check && allow_panic) {
		msyslog(LOG_ERR, "adj_systime: allow_panic is TRUE!");
	}
	return TRUE;
}
#endif


/*
 * step_systime - step the system clock.
 */

int
step_systime(
	double step
	)
{
	time_t pivot; /* for ntp era unfolding */
	struct timeval timetv, tvlast, tvdiff;
	struct timespec timets;
	struct calendar jd;
	l_fp fp_ofs, fp_sys; /* offset and target system time in FP */

	/*
	 * Get pivot time for NTP era unfolding. Since we don't step
	 * very often, we can afford to do the whole calculation from
	 * scratch. And we're not in the time-critical path yet.
	 */
#if SIZEOF_TIME_T > 4
	/*
	 * This code makes sure the resulting time stamp for the new
	 * system time is in the 2^32 seconds starting at 1970-01-01,
	 * 00:00:00 UTC.
	 */
	pivot = 0x80000000;
#if USE_COMPILETIME_PIVOT
	/*
	 * Add the compile time minus 10 years to get a possible target
	 * area of (compile time - 10 years) to (compile time + 126
	 * years).  This should be sufficient for a given binary of
	 * NTPD.
	 */
	if (ntpcal_get_build_date(&jd)) {
		jd.year -= 10;
		pivot += ntpcal_date_to_time(&jd);
	} else {
		msyslog(LOG_ERR,
			"step-systime: assume 1970-01-01 as build date");
	}
#else
	UNUSED_LOCAL(jd);
#endif /* USE_COMPILETIME_PIVOT */
#else
	UNUSED_LOCAL(jd);
	/* This makes sure the resulting time stamp is on or after
	 * 1969-12-31/23:59:59 UTC and gives us additional two years,
	 * from the change of NTP era in 2036 to the UNIX rollover in
	 * 2038. (Minus one second, but that won't hurt.) We *really*
	 * need a longer 'time_t' after that!  Or a different baseline,
	 * but that would cause other serious trouble, too.
	 */
	pivot = 0x7FFFFFFF;
#endif

	/* get the complete jump distance as l_fp */
	DTOLFP(sys_residual, &fp_sys);
	DTOLFP(step,         &fp_ofs);
	L_ADD(&fp_ofs, &fp_sys);

	/* ---> time-critical path starts ---> */

	/* get the current time as l_fp (without fuzz) and as struct timeval */
	get_ostime(&timets);
	fp_sys = tspec_stamp_to_lfp(timets);
	tvlast.tv_sec = timets.tv_sec;
	tvlast.tv_usec = (timets.tv_nsec + 500) / 1000;

	/* get the target time as l_fp */
	L_ADD(&fp_sys, &fp_ofs);

	/* unfold the new system time */
	timetv = lfp_stamp_to_tval(fp_sys, &pivot);

	/* now set new system time */
	if (ntp_set_tod(&timetv, NULL) != 0) {
		msyslog(LOG_ERR, "step-systime: %m");
		if (enable_panic_check && allow_panic) {
			msyslog(LOG_ERR, "step_systime: allow_panic is TRUE!");
		}
		return FALSE;
	}

	/* <--- time-critical path ended with 'ntp_set_tod()' <--- */

	sys_residual = 0;
	lamport_violated = (step < 0);
	if (step_callback)
		(*step_callback)();

#ifdef NEED_HPUX_ADJTIME
	/*
	 * CHECKME: is this correct when called by ntpdate?????
	 */
	_clear_adjtime();
#endif

	/*
	 * FreeBSD, for example, has:
	 * struct utmp {
	 *	   char    ut_line[UT_LINESIZE];
	 *	   char    ut_name[UT_NAMESIZE];
	 *	   char    ut_host[UT_HOSTSIZE];
	 *	   long    ut_time;
	 * };
	 * and appends line="|", name="date", host="", time for the OLD
	 * and appends line="{", name="date", host="", time for the NEW // }
	 * to _PATH_WTMP .
	 *
	 * Some OSes have utmp, some have utmpx.
	 */

	/*
	 * Write old and new time entries in utmp and wtmp if step
	 * adjustment is greater than one second.
	 *
	 * This might become even Uglier...
	 */
	tvdiff = abs_tval(sub_tval(timetv, tvlast));
	if (tvdiff.tv_sec > 0) {
#ifdef HAVE_UTMP_H
		struct utmp ut;
#endif
#ifdef HAVE_UTMPX_H
		struct utmpx utx;
#endif

#ifdef HAVE_UTMP_H
		ZERO(ut);
#endif
#ifdef HAVE_UTMPX_H
		ZERO(utx);
#endif

		/* UTMP */

#ifdef UPDATE_UTMP
# ifdef HAVE_PUTUTLINE
#  ifndef _PATH_UTMP
#   define _PATH_UTMP UTMP_FILE
#  endif
		utmpname(_PATH_UTMP);
		ut.ut_type = OLD_TIME;
		strlcpy(ut.ut_line, OTIME_MSG, sizeof(ut.ut_line));
		ut.ut_time = tvlast.tv_sec;
		setutent();
		pututline(&ut);
		ut.ut_type = NEW_TIME;
		strlcpy(ut.ut_line, NTIME_MSG, sizeof(ut.ut_line));
		ut.ut_time = timetv.tv_sec;
		setutent();
		pututline(&ut);
		endutent();
# else /* not HAVE_PUTUTLINE */
# endif /* not HAVE_PUTUTLINE */
#endif /* UPDATE_UTMP */

		/* UTMPX */

#ifdef UPDATE_UTMPX
# ifdef HAVE_PUTUTXLINE
		utx.ut_type = OLD_TIME;
		strlcpy(utx.ut_line, OTIME_MSG, sizeof(utx.ut_line));
		utx.ut_tv = tvlast;
		setutxent();
		pututxline(&utx);
		utx.ut_type = NEW_TIME;
		strlcpy(utx.ut_line, NTIME_MSG, sizeof(utx.ut_line));
		utx.ut_tv = timetv;
		setutxent();
		pututxline(&utx);
		endutxent();
# else /* not HAVE_PUTUTXLINE */
# endif /* not HAVE_PUTUTXLINE */
#endif /* UPDATE_UTMPX */

		/* WTMP */

#ifdef UPDATE_WTMP
# ifdef HAVE_PUTUTLINE
#  ifndef _PATH_WTMP
#   define _PATH_WTMP WTMP_FILE
#  endif
		utmpname(_PATH_WTMP);
		ut.ut_type = OLD_TIME;
		strlcpy(ut.ut_line, OTIME_MSG, sizeof(ut.ut_line));
		ut.ut_time = tvlast.tv_sec;
		setutent();
		pututline(&ut);
		ut.ut_type = NEW_TIME;
		strlcpy(ut.ut_line, NTIME_MSG, sizeof(ut.ut_line));
		ut.ut_time = timetv.tv_sec;
		setutent();
		pututline(&ut);
		endutent();
# else /* not HAVE_PUTUTLINE */
# endif /* not HAVE_PUTUTLINE */
#endif /* UPDATE_WTMP */

		/* WTMPX */

#ifdef UPDATE_WTMPX
# ifdef HAVE_PUTUTXLINE
		utx.ut_type = OLD_TIME;
		utx.ut_tv = tvlast;
		strlcpy(utx.ut_line, OTIME_MSG, sizeof(utx.ut_line));
#  ifdef HAVE_UPDWTMPX
		updwtmpx(WTMPX_FILE, &utx);
#  else /* not HAVE_UPDWTMPX */
#  endif /* not HAVE_UPDWTMPX */
# else /* not HAVE_PUTUTXLINE */
# endif /* not HAVE_PUTUTXLINE */
# ifdef HAVE_PUTUTXLINE
		utx.ut_type = NEW_TIME;
		utx.ut_tv = timetv;
		strlcpy(utx.ut_line, NTIME_MSG, sizeof(utx.ut_line));
#  ifdef HAVE_UPDWTMPX
		updwtmpx(WTMPX_FILE, &utx);
#  else /* not HAVE_UPDWTMPX */
#  endif /* not HAVE_UPDWTMPX */
# else /* not HAVE_PUTUTXLINE */
# endif /* not HAVE_PUTUTXLINE */
#endif /* UPDATE_WTMPX */

	}
	if (enable_panic_check && allow_panic) {
		msyslog(LOG_ERR, "step_systime: allow_panic is TRUE!");
		INSIST(!allow_panic);
	}
	return TRUE;
}

#endif	/* !SIM */
