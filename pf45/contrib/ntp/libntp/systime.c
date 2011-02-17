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

#ifdef SIM
# include "ntpsim.h"
#endif /*SIM */

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_UTMP_H
# include <utmp.h>
#endif /* HAVE_UTMP_H */
#ifdef HAVE_UTMPX_H
# include <utmpx.h>
#endif /* HAVE_UTMPX_H */

/*
 * These routines (get_systime, step_systime, adj_systime) implement an
 * interface between the system independent NTP clock and the Unix
 * system clock in various architectures and operating systems.
 *
 * Time is a precious quantity in these routines and every effort is
 * made to minimize errors by always rounding toward zero and amortizing
 * adjustment residues. By default the adjustment quantum is 1 us for
 * the usual Unix tickadj() system call, but this can be increased if
 * necessary by the tick configuration command. For instance, when the
 * adjtime() quantum is a clock tick for a 100-Hz clock, the quantum
 * should be 10 ms.
 */
#if defined RELIANTUNIX_CLOCK || defined SCO5_CLOCK
double	sys_tick = 10e-3;	/* 10 ms tickadj() */
#else
double	sys_tick = 1e-6;	/* 1 us tickadj() */
#endif
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
	double dtemp;

#if defined(HAVE_CLOCK_GETTIME) || defined(HAVE_GETCLOCK)
	struct timespec ts;	/* seconds and nanoseconds */

	/*
	 * Convert Unix clock from seconds and nanoseconds to seconds.
	 * The bottom is only two bits down, so no need for fuzz.
	 * Some systems don't have that level of precision, however...
	 */
# ifdef HAVE_CLOCK_GETTIME
	clock_gettime(CLOCK_REALTIME, &ts);
# else
	getclock(TIMEOFDAY, &ts);
# endif
	now->l_i = ts.tv_sec + JAN_1970;
	dtemp = ts.tv_nsec / 1e9;

#else /* HAVE_CLOCK_GETTIME || HAVE_GETCLOCK */
	struct timeval tv;	/* seconds and microseconds */

	/*
	 * Convert Unix clock from seconds and microseconds to seconds.
	 * Add in unbiased random fuzz beneath the microsecond.
	 */
	GETTIMEOFDAY(&tv, NULL);
	now->l_i = tv.tv_sec + JAN_1970;
	dtemp = tv.tv_usec / 1e6;

#endif /* HAVE_CLOCK_GETTIME || HAVE_GETCLOCK */

	/*
	 * ntp_random() produces 31 bits (always nonnegative).
	 * This bit is done only after the precision has been
	 * determined.
	 */
	if (sys_precision != 0)
		dtemp += (ntp_random() / FRAC - .5) / (1 <<
		    -sys_precision);

	/*
	 * Renormalize to seconds past 1900 and fraction.
	 */
	dtemp += sys_residual;
	if (dtemp >= 1) {
		dtemp -= 1;
		now->l_i++;
	} else if (dtemp < 0) {
		dtemp += 1;
		now->l_i--;
	}
	dtemp *= FRAC;
	now->l_uf = (u_int32)dtemp;
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
/*
 * get_systime - return the system time in NTP timestamp format 
 */
void
get_systime(
        l_fp *now		/* current system time in l_fp */        )
{
	/*
	 * To fool the code that determines the local clock precision,
	 * we advance the clock a minimum of 200 nanoseconds on every
	 * clock read. This is appropriate for a typical modern machine
	 * with nanosecond clocks. Note we make no attempt here to
	 * simulate reading error, since the error is so small. This may
	 * change when the need comes to implement picosecond clocks.
	 */
	if (ntp_node.ntp_time == ntp_node.last_time)
		ntp_node.ntp_time += 200e-9;
	ntp_node.last_time = ntp_node.ntp_time;
	DTOLFP(ntp_node.ntp_time, now);
}
 
 
/*
 * adj_systime - advance or retard the system clock exactly like the
 * real thng.
 */
int				/* always succeeds */
adj_systime(
        double now		/* time adjustment (s) */
        )
{
	struct timeval adjtv;	/* new adjustment */
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
	ntp_node.adj = now;
	return (1);
}
 
 
/*
 * step_systime - step the system clock. We are religious here.
 */
int				/* always succeeds */
step_systime(
        double now		/* step adjustment (s) */
        )
{
#ifdef DEBUG
	if (debug)
		printf("step_systime: time %.6f adj %.6f\n",
		   ntp_node.ntp_time, now);
#endif
	ntp_node.ntp_time += now;
	return (1);
}

/*
 * node_clock - update the clocks
 */
int				/* always succeeds */
node_clock(
	Node *n,		/* global node pointer */
	double t		/* node time */
	)
{
	double	dtemp;

	/*
	 * Advance client clock (ntp_time). Advance server clock
	 * (clk_time) adjusted for systematic and random frequency
	 * errors. The random error is a random walk computed as the
	 * integral of samples from a Gaussian distribution.
	 */
	dtemp = t - n->ntp_time;
	n->time = t;
	n->ntp_time += dtemp;
	n->ferr += gauss(0, dtemp * n->fnse);
	n->clk_time += dtemp * (1 + n->ferr);

	/*
	 * Perform the adjtime() function. If the adjustment completed
	 * in the previous interval, amortize the entire amount; if not,
	 * carry the leftover to the next interval.
	 */
	dtemp *= n->slew;
	if (dtemp < fabs(n->adj)) {
		if (n->adj < 0) {
			n->adj += dtemp;
			n->ntp_time -= dtemp;
		} else {
			n->adj -= dtemp;
			n->ntp_time += dtemp;
		}
	} else {
		n->ntp_time += n->adj;
		n->adj = 0;
	}
        return (0);
}

 
/*
 * gauss() - returns samples from a gaussion distribution
 */
double				/* Gaussian sample */
gauss(
	double m,		/* sample mean */
	double s		/* sample standard deviation (sigma) */
	)
{
        double q1, q2;

	/*
	 * Roll a sample from a Gaussian distribution with mean m and
	 * standard deviation s. For m = 0, s = 1, mean(y) = 0,
	 * std(y) = 1.
	 */
	if (s == 0)
		return (m);
        while ((q1 = drand48()) == 0);
        q2 = drand48();
        return (m + s * sqrt(-2. * log(q1)) * cos(2. * PI * q2));
}

 
/*
 * poisson() - returns samples from a network delay distribution
 */
double				/* delay sample (s) */
poisson(
	double m,		/* fixed propagation delay (s) */
	double s		/* exponential parameter (mu) */
	)
{
        double q1;

	/*
	 * Roll a sample from a composite distribution with propagation
	 * delay m and exponential distribution time with parameter s.
	 * For m = 0, s = 1, mean(y) = std(y) = 1.
	 */
	if (s == 0)
		return (m);
        while ((q1 = drand48()) == 0);
        return (m - s * log(q1 * s));
}
#endif /* SIM */
