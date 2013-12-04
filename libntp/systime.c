/*
 * systime -- routines to fiddle a UNIX clock.
 *
 * ATTENTION: Get approval from Dave Mills on all changes to this file!
 *
 */
#include "ntp_machine.h"
#include "ntp_fp.h"
#include "ntp_syslog.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"
#include "ntp_random.h"
#include "ntpd.h"		/* for sys_precision */

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_UTMP_H
# include <utmp.h>
#endif /* HAVE_UTMP_H */
#ifdef HAVE_UTMPX_H
# include <utmpx.h>
#endif /* HAVE_UTMPX_H */


#define	FUZZ	500e-6		/* fuzz pivot */

/*
 * These routines (get_systime, step_systime, adj_systime) implement an
 * interface between the system independent NTP clock and the Unix
 * system clock in various architectures and operating systems. Time is
 * a precious quantity in these routines and every effort is made to
 * minimize errors by unbiased rounding and amortizing adjustment
 * residues.
 *
 * In order to improve the apparent resolution, provide unbiased
 * rounding and insure that the readings cannot be predicted, the low-
 * order unused portion of the time below the resolution limit is filled
 * with an unbiased random fuzz.
 *
 * The sys_tick variable secifies the system clock tick interval in
 * seconds. For systems that can interpolate between timer interrupts,
 * the resolution is presumed much less than the time to read the system
 * clock, which is the value of sys_tick after the precision has been
 * determined. For those systems that cannot interpolate between timer
 * interrupts, sys_tick will be much larger in the order of 10 ms, so the
 * fuzz should be that value. For Sunses the tick is not interpolated, but
 * the system clock is derived from a 2-MHz oscillator, so the resolution
 * is 500 ns and sys_tick is 500 ns.
 */
double	sys_tick = 0;		/* precision (time to read the clock) */
double	sys_residual = 0;	/* adjustment residue (s) */

#ifndef SIM

/*
 * get_systime - return system time in NTP timestamp format.
 */
void
get_systime(
	l_fp *now		/* system time */
	)
{
	double	dtemp;

#if defined(HAVE_CLOCK_GETTIME) || defined(HAVE_GETCLOCK)
	struct timespec ts;	/* seconds and nanoseconds */

	/*
	 * Convert Unix timespec from seconds and nanoseconds to NTP
	 * seconds and fraction.
	 */
# ifdef HAVE_CLOCK_GETTIME
	clock_gettime(CLOCK_REALTIME, &ts);
# else
	getclock(TIMEOFDAY, &ts);
# endif
	now->l_i = (int32)ts.tv_sec + JAN_1970;
	dtemp = 0;
	if (sys_tick > FUZZ)
		dtemp = ntp_random() * 2. / FRAC * sys_tick * 1e9;
	else if (sys_tick > 0)
		dtemp = ntp_random() * 2. / FRAC;
	dtemp = (ts.tv_nsec + dtemp) * 1e-9;
	if (dtemp >= 1.) {
		dtemp -= 1.;
		now->l_i++;
	} else if (dtemp < 0) {
		dtemp += 1.;
		now->l_i--;
	}
	now->l_uf = (u_int32)(dtemp * FRAC);

#else /* HAVE_CLOCK_GETTIME || HAVE_GETCLOCK */
	struct timeval tv;	/* seconds and microseconds */

	/*
	 * Convert Unix timeval from seconds and microseconds to NTP
	 * seconds and fraction.
	 */
	GETTIMEOFDAY(&tv, NULL);
	now->l_i = tv.tv_sec + JAN_1970;
	dtemp = 0;
	if (sys_tick > FUZZ)
		dtemp = ntp_random() * 2. / FRAC * sys_tick * 1e6;
	else if (sys_tick > 0)
		dtemp = ntp_random() * 2. / FRAC;
	dtemp = (tv.tv_usec + dtemp) * 1e-6;
	if (dtemp >= 1.) {
		dtemp -= 1.;
		now->l_i++;
	} else if (dtemp < 0) {
		dtemp += 1.;
		now->l_i--;
	}
	now->l_uf = (u_int32)(dtemp * FRAC);

#endif /* HAVE_CLOCK_GETTIME || HAVE_GETCLOCK */
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
	double	dtemp;
	long	ticks;
	int	isneg = 0;

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
	ticks = (long)(dtemp / sys_tick + .5);
	adjtv.tv_usec = (long)(ticks * sys_tick * 1e6);
	dtemp -= adjtv.tv_usec / 1e6;
	sys_residual = dtemp;

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
			return (0);
		}
	}
	return (1);
}
#endif


/*
 * step_systime - step the system clock.
 */
int
step_systime(
	double now
	)
{
	struct timeval timetv, adjtv, oldtimetv;
	int isneg = 0;
	double dtemp;
#if defined(HAVE_CLOCK_GETTIME) || defined(HAVE_GETCLOCK)
	struct timespec ts;
#endif

	dtemp = sys_residual + now;
	if (dtemp < 0) {
		isneg = 1;
		dtemp = - dtemp;
		adjtv.tv_sec = (int32)dtemp;
		adjtv.tv_usec = (u_int32)((dtemp -
		    (double)adjtv.tv_sec) * 1e6 + .5);
	} else {
		adjtv.tv_sec = (int32)dtemp;
		adjtv.tv_usec = (u_int32)((dtemp -
		    (double)adjtv.tv_sec) * 1e6 + .5);
	}
#if defined(HAVE_CLOCK_GETTIME) || defined(HAVE_GETCLOCK)
# ifdef HAVE_CLOCK_GETTIME
	(void) clock_gettime(CLOCK_REALTIME, &ts);
# else
	(void) getclock(TIMEOFDAY, &ts);
# endif
	timetv.tv_sec = ts.tv_sec;
	timetv.tv_usec = ts.tv_nsec / 1000;
#else /*  not HAVE_GETCLOCK */
	(void) GETTIMEOFDAY(&timetv, (struct timezone *)0);
#endif /* not HAVE_GETCLOCK */

	oldtimetv = timetv;

#ifdef DEBUG
	if (debug)
		printf("step_systime: step %.6f residual %.6f\n", now, sys_residual);
#endif
	if (isneg) {
		timetv.tv_sec -= adjtv.tv_sec;
		timetv.tv_usec -= adjtv.tv_usec;
		if (timetv.tv_usec < 0) {
			timetv.tv_sec--;
			timetv.tv_usec += 1000000;
		}
	} else {
		timetv.tv_sec += adjtv.tv_sec;
		timetv.tv_usec += adjtv.tv_usec;
		if (timetv.tv_usec >= 1000000) {
			timetv.tv_sec++;
			timetv.tv_usec -= 1000000;
		}
	}
	if (ntp_set_tod(&timetv, NULL) != 0) {
		msyslog(LOG_ERR, "step-systime: %m");
		return (0);
	}
	sys_residual = 0;

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
	 * and appends line="{", name="date", host="", time for the NEW
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
	if (oldtimetv.tv_sec != timetv.tv_sec)
	{
#ifdef HAVE_UTMP_H
		struct utmp ut;
#endif
#ifdef HAVE_UTMPX_H
		struct utmpx utx;
#endif

#ifdef HAVE_UTMP_H
		memset((char *)&ut, 0, sizeof(ut));
#endif
#ifdef HAVE_UTMPX_H
		memset((char *)&utx, 0, sizeof(utx));
#endif

		/* UTMP */

#ifdef UPDATE_UTMP
# ifdef HAVE_PUTUTLINE
		ut.ut_type = OLD_TIME;
		(void)strcpy(ut.ut_line, OTIME_MSG);
		ut.ut_time = oldtimetv.tv_sec;
		pututline(&ut);
		setutent();
		ut.ut_type = NEW_TIME;
		(void)strcpy(ut.ut_line, NTIME_MSG);
		ut.ut_time = timetv.tv_sec;
		pututline(&ut);
		endutent();
# else /* not HAVE_PUTUTLINE */
# endif /* not HAVE_PUTUTLINE */
#endif /* UPDATE_UTMP */

		/* UTMPX */

#ifdef UPDATE_UTMPX
# ifdef HAVE_PUTUTXLINE
		utx.ut_type = OLD_TIME;
		(void)strcpy(utx.ut_line, OTIME_MSG);
		utx.ut_tv = oldtimetv;
		pututxline(&utx);
		setutxent();
		utx.ut_type = NEW_TIME;
		(void)strcpy(utx.ut_line, NTIME_MSG);
		utx.ut_tv = timetv;
		pututxline(&utx);
		endutxent();
# else /* not HAVE_PUTUTXLINE */
# endif /* not HAVE_PUTUTXLINE */
#endif /* UPDATE_UTMPX */

		/* WTMP */

#ifdef UPDATE_WTMP
# ifdef HAVE_PUTUTLINE
		utmpname(WTMP_FILE);
		ut.ut_type = OLD_TIME;
		(void)strcpy(ut.ut_line, OTIME_MSG);
		ut.ut_time = oldtimetv.tv_sec;
		pututline(&ut);
		ut.ut_type = NEW_TIME;
		(void)strcpy(ut.ut_line, NTIME_MSG);
		ut.ut_time = timetv.tv_sec;
		pututline(&ut);
		endutent();
# else /* not HAVE_PUTUTLINE */
# endif /* not HAVE_PUTUTLINE */
#endif /* UPDATE_WTMP */

		/* WTMPX */

#ifdef UPDATE_WTMPX
# ifdef HAVE_PUTUTXLINE
		utx.ut_type = OLD_TIME;
		utx.ut_tv = oldtimetv;
		(void)strcpy(utx.ut_line, OTIME_MSG);
#  ifdef HAVE_UPDWTMPX
		updwtmpx(WTMPX_FILE, &utx);
#  else /* not HAVE_UPDWTMPX */
#  endif /* not HAVE_UPDWTMPX */
# else /* not HAVE_PUTUTXLINE */
# endif /* not HAVE_PUTUTXLINE */
# ifdef HAVE_PUTUTXLINE
		utx.ut_type = NEW_TIME;
		utx.ut_tv = timetv;
		(void)strcpy(utx.ut_line, NTIME_MSG);
#  ifdef HAVE_UPDWTMPX
		updwtmpx(WTMPX_FILE, &utx);
#  else /* not HAVE_UPDWTMPX */
#  endif /* not HAVE_UPDWTMPX */
# else /* not HAVE_PUTUTXLINE */
# endif /* not HAVE_PUTUTXLINE */
#endif /* UPDATE_WTMPX */

	}
	return (1);
}

#else /* SIM */
/*
 * Clock routines for the simulator - Harish Nair, with help
 */


/* SK: 
 * The code that used to be here has been moved to ntpsim.c,
 * where, IMHO, it rightfully belonged.
 */

#endif
