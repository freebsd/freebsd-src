/* systime.c,v 3.1 1993/07/06 01:08:46 jbj Exp
 * systime -- routines to fiddle a UNIX clock.
 */
#include <sys/types.h>
#include <sys/time.h>
#if defined(SYS_HPUX) || defined(sgi) || defined(SYS_BSDI)
#include <sys/param.h>
#include <utmp.h>
#endif

#ifdef SYS_LINUX
#include <sys/timex.h>
#endif

#include "ntp_fp.h"
#include "ntp_syslog.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

#if defined(STEP_SLEW)
#define SLEWALWAYS
#endif

extern int debug;

/*
 * These routines (init_systime, get_systime, step_systime, adj_systime)
 * implement an interface between the (more or less) system independent
 * bits of NTP and the peculiarities of dealing with the Unix system
 * clock.  These routines will run with good precision fairly independently
 * of your kernel's value of tickadj.  I couldn't tell the difference
 * between tickadj==40 and tickadj==5 on a microvax, though I prefer
 * to set tickadj == 500/hz when in doubt.  At your option you
 * may compile this so that your system's clock is always slewed to the
 * correct time even for large corrections.  Of course, all of this takes
 * a lot of code which wouldn't be needed with a reasonable tickadj and
 * a willingness to let the clock be stepped occasionally.  Oh well.
 */

/*
 * Clock variables.  We round calls to adjtime() to adj_precision
 * microseconds, and limit the adjustment to tvu_maxslew microseconds
 * (tsf_maxslew fractional sec) in one adjustment interval.  As we are
 * thus limited in the speed and precision with which we can adjust the
 * clock, we compensate by keeping the known "error" in the system time
 * in sys_clock_offset.  This is added to timestamps returned by get_systime().
 * We also remember the clock precision we computed from the kernel in
 * case someone asks us.
 */
	LONG adj_precision;	/* adj precision in usec (tickadj) */
	LONG tvu_maxslew;	/* maximum adjust doable in 1<<CLOCK_ADJ sec (usec) */

	U_LONG tsf_maxslew;	/* same as above, as LONG format */

	LONG sys_clock;
	l_fp sys_clock_offset;	/* correction for current system time */

/*
 * get_systime - return the system time in timestamp format
 * As a side effect, update sys_clock.
 */
void
get_systime(ts)
	l_fp *ts;
{
	struct timeval tv;

#if !defined(SLEWALWAYS)
	/*
	 * Quickly get the time of day and convert it
	 */
	(void) GETTIMEOFDAY(&tv, (struct timezone *)0);
	TVTOTS(&tv, ts);
	ts->l_uf += TS_ROUNDBIT;	/* guaranteed not to overflow */
#else
	/*
	 * Get the time of day, convert to time stamp format
	 * and add in the current time offset.  Then round
	 * appropriately.
	 */
	(void) GETTIMEOFDAY(&tv, (struct timezone *)0);
	TVTOTS(&tv, ts);
	L_ADD(ts, &sys_clock_offset);
	if (ts->l_uf & TS_ROUNDBIT)
		L_ADDUF(ts, (unsigned LONG) TS_ROUNDBIT);
#endif	/* !defined(SLEWALWAYS) */
	ts->l_ui += JAN_1970;
	ts->l_uf &= TS_MASK;

	sys_clock = ts->l_ui;
}

/*
 * step_systime - do a step adjustment in the system time (at least from
 *		  NTP's point of view.
 */
int
step_systime(ts)
	l_fp *ts;
{
#ifdef SLEWALWAYS 
#ifdef STEP_SLEW
	register U_LONG tmp_ui;
	register U_LONG tmp_uf;
	int isneg;
	int n;

	/*
	 * Take the absolute value of the offset
	 */
	tmp_ui = ts->l_ui;
	tmp_uf = ts->l_uf;
	if (M_ISNEG(tmp_ui, tmp_uf)) {
		M_NEG(tmp_ui, tmp_uf);
		isneg = 1;
	} else
		isneg = 0;

	if (tmp_ui >= 3) {		/* Step it and slew we  might win */
             n = step_systime_real(ts);
	     if (!n) return n;
	     if (isneg) 
		ts->l_ui = ~0;
	     else
		ts->l_ui = ~0;
	}
#endif
        /*
         * Just add adjustment into the current offset.  The update
         * routine will take care of bringing the system clock into
         * line.
         */
	L_ADD(&sys_clock_offset, ts);
	return 1;
#else /* SLEWALWAYS  */
        return step_systime_real(ts);
#endif	/* SLEWALWAYS */
}

int	max_no_complete	= 20;

/*
 * adj_systime - called once every 1<<CLOCK_ADJ seconds to make system time
 *		 adjustments.
 */
int
adj_systime(ts)
	l_fp *ts;
{
	register unsigned LONG offset_i, offset_f;
	register LONG temp;
	register unsigned LONG residual;
	register int isneg = 0;
	struct timeval adjtv, oadjtv;
	l_fp oadjts;
	LONG adj = ts->l_f;
	int rval;

	adjtv.tv_sec = adjtv.tv_usec = 0;

	/*
	 * Move the current offset into the registers
	 */
	offset_i = sys_clock_offset.l_ui;
	offset_f = sys_clock_offset.l_uf;

	/*
	 * Add the new adjustment into the system offset.  Adjust the
	 * system clock to minimize this.
	 */
	M_ADDF(offset_i, offset_f, adj);
	if (M_ISNEG(offset_i, offset_f)) {
		isneg = 1;
		M_NEG(offset_i, offset_f);
	}
#ifdef DEBUG
	if (debug > 4)
		syslog(LOG_DEBUG, "adj_systime(%s): offset = %s%s\n",
		    mfptoa((adj<0?-1:0), adj, 9), isneg?"-":"",
		    umfptoa(offset_i, offset_f, 9));
#endif

	adjtv.tv_sec = 0;
	if (offset_i > 0 || offset_f >= tsf_maxslew) {
		/*
		 * Slew is bigger than we can complete in
		 * the adjustment interval.  Make a maximum
		 * sized slew and reduce sys_clock_offset by this
		 * much.
		 */
		M_SUBUF(offset_i, offset_f, tsf_maxslew);
		if (!isneg) {
			adjtv.tv_usec = tvu_maxslew;
		} else {
			adjtv.tv_usec = -tvu_maxslew;
			M_NEG(offset_i, offset_f);
		}

#ifdef DEBUG
		if (debug > 4)
			syslog(LOG_DEBUG,
			    "maximum slew: %s%s, remainder = %s\n",
			    isneg?"-":"", umfptoa(0, tsf_maxslew, 9),
			    mfptoa(offset_i, offset_f, 9));
#endif
	} else {
		/*
		 * We can do this slew in the time period.  Do our
		 * best approximation (rounded), save residual for
		 * next adjustment.
		 *
		 * Note that offset_i is guaranteed to be 0 here.
		 */
		TSFTOTVU(offset_f, temp);
#ifndef ADJTIME_IS_ACCURATE
		/*
		 * Round value to be an even multiple of adj_precision
		 */
		residual = temp % adj_precision;
		temp -= residual;
		if (residual << 1 >= adj_precision)
			temp += adj_precision;
#endif /* ADJTIME_IS_ACCURATE */
		TVUTOTSF(temp, residual);
		M_SUBUF(offset_i, offset_f, residual);
		if (isneg) {
			adjtv.tv_usec = -temp;
			M_NEG(offset_i, offset_f);
		} else {
			adjtv.tv_usec = temp;
		}
#ifdef DEBUG
		if (debug > 4)
			syslog(LOG_DEBUG,
		"slew adjtv = %s, adjts = %s, sys_clock_offset = %s\n",
			    tvtoa(&adjtv), umfptoa(0, residual, 9),
			    mfptoa(offset_i, offset_f, 9));
#endif
	}

	if (adjtime(&adjtv, &oadjtv) < 0) {
		syslog(LOG_ERR, "Can't do time adjustment: %m");
		rval = 0;
	} else {
		sys_clock_offset.l_ui = offset_i;
		sys_clock_offset.l_uf = offset_f;
		rval = 1;

#ifdef DEBUGRS6000
		syslog(LOG_ERR, "adj_systime(%s): offset = %s%s\n",
			mfptoa((adj<0?-1:0), adj, 9), isneg?"-":"",
			umfptoa(offset_i, offset_f, 9));
		syslog(LOG_ERR, "%d %d %d %d\n", (int) adjtv.tv_sec,
			(int) adjtv.tv_usec, (int) oadjtv.tv_sec, (int)
			oadjtv.tv_usec);
#endif /* DEBUGRS6000 */

		if (oadjtv.tv_sec != 0 || oadjtv.tv_usec != 0) {
			sTVTOTS(&oadjtv, &oadjts);
			L_ADD(&sys_clock_offset, &oadjts);
			if (max_no_complete > 0) {
				syslog(LOG_WARNING,
				    "Previous time adjustment didn't complete");
#ifdef DEBUG
				if (debug > 4)
					syslog(LOG_DEBUG,
					    "Previous adjtime() incomplete, residual = %s\n",
					    tvtoa(&oadjtv));
#endif
				if (--max_no_complete == 0)
					syslog(LOG_WARNING,
					    "*** No more 'Prev time adj didn't complete'");
			}
		}
	}
	return(rval);
}


/*
 * This is used by ntpdate even when xntpd does not use it! WLJ
 */
int
step_systime_real(ts)
	l_fp *ts;
{
	struct timeval timetv, adjtv;
	int isneg = 0;
#if defined(SYS_HPUX)
	struct utmp ut;
	time_t oldtime;
#endif

	/*
	 * We can afford to be sloppy here since if this is called
	 * the time is really screwed and everything is being reset.
	 */
	L_ADD(&sys_clock_offset, ts);

	if (L_ISNEG(&sys_clock_offset)) {
		isneg = 1;
		L_NEG(&sys_clock_offset);
	}
	TSTOTV(&sys_clock_offset, &adjtv);

	(void) GETTIMEOFDAY(&timetv, (struct timezone *)0);
#if defined(SYS_HPUX)
	oldtime = timetv.tv_sec;
#endif
#ifdef DEBUG
	if (debug > 3)
		syslog(LOG_DEBUG, "step: %s, sys_clock_offset = %s, adjtv = %s, timetv = %s\n",
		    lfptoa(ts, 9), lfptoa(&sys_clock_offset, 9), tvtoa(&adjtv),
		    utvtoa(&timetv));
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
	if (SETTIMEOFDAY(&timetv, (struct timezone *)0) != 0) {
		syslog(LOG_ERR, "Can't set time of day: %m");
		return 0;
	}
#ifdef DEBUG
	if (debug > 3)
		syslog(LOG_DEBUG, "step: new timetv = %s\n", utvtoa(&timetv));
#endif
	sys_clock_offset.l_ui = sys_clock_offset.l_uf = 0;
#if defined(SYS_HPUX)
#if (SYS_HPUX < 10)
	/*
	 * CHECKME: is this correct when called by ntpdate?????
	 */
	_clear_adjtime();
#endif
	/*
	 * Write old and new time entries in utmp and wtmp if step adjustment
	 * is greater than one second.
	 */
	if (oldtime != timetv.tv_sec) {
		memset((char *)&ut, 0, sizeof(ut));
		ut.ut_type = OLD_TIME;
		ut.ut_time = oldtime;
		(void)strcpy(ut.ut_line, OTIME_MSG);
		pututline(&ut);
		setutent();
		ut.ut_type = NEW_TIME;
		ut.ut_time = timetv.tv_sec;
		(void)strcpy(ut.ut_line, NTIME_MSG);
		pututline(&ut);
		utmpname(WTMP_FILE);
		ut.ut_type = OLD_TIME;
		ut.ut_time = oldtime;
		(void)strcpy(ut.ut_line, OTIME_MSG);
		pututline(&ut);
		ut.ut_type = NEW_TIME;
		ut.ut_time = timetv.tv_sec;
		(void)strcpy(ut.ut_line, NTIME_MSG);
		pututline(&ut);
		endutent();
	}
#endif
	return 1;
}
