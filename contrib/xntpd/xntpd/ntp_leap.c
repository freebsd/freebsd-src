/* ntp_leap.c,v 3.1 1993/07/06 01:11:18 jbj Exp
 * ntp_leap - maintain leap bits and take action when a leap occurs
 */
#include <stdio.h>

#include "ntpd.h"
#include "ntp_stdlib.h"

/*
 * This module is devoted to maintaining the leap bits and taking
 * action when a leap second occurs.  It probably has bugs since
 * a leap second has never occurred to excercise the code.
 *
 * The code does two things when a leap second occurs.  It first
 * steps the clock one second in the appropriate direction.  It
 * then informs the reference clock code, if compiled in, that the
 * leap second has occured so that any clocks which need to disable
 * themselves can do so.  This is done within the first few seconds
 * after midnight, UTC.
 *
 * The code maintains two variables which may be written externally,
 * leap_warning and leap_indicator.  Leap_warning can be written
 * any time in the month preceeding a leap second.  24 hours before
 * the leap is to occur, leap_warning's contents are copied to
 * leap_indicator.  The latter is used by reference clocks to set
 * their leap bits.
 *
 * The module normally maintains a timer which is arranged to expire
 * just after 0000Z one day before the leap.  On the day a leap might
 * occur the interrupt is aimed at 2200Z and every 5 minutes thereafter
 * until 1200Z to see if the leap bits appear.
 */

/*
 * The leap indicator and leap warning flags.  Set by control messages
 */
u_char leap_indicator;
u_char leap_warning;
u_char leap_mask;		/* set on day before a potential leap */
/*
 * Timer.  The timer code imports this so it can call us prior to
 * calling out any pending transmits.
 */
U_LONG leap_timer;

/*
 * We don't want to do anything drastic if the leap function is handled
 * by the kernel.
 */
extern int pll_control;		/* set nonzero if kernel pll in uss */

/*
 * Internal leap bits.  If we see leap bits set during the last
 * hour we set these.
 */
u_char leapbits;

/*
 * Constants.
 */
#define	OKAYTOSETWARNING	(31*24*60*60)
#define	DAYBEFORE		(24*60*60)
#define	TWOHOURSBEFORE		(2*60*60)
#define	FIVEMINUTES		(5*60)
#define	ONEMINUTE		(60)

/*
 * Imported from the timer module.
 */
extern U_LONG current_time;


/*
 * Some statistics counters
 */
U_LONG leap_processcalls;	/* calls to leap_process */
U_LONG leap_notclose;		/* leap found to be a LONG time from now */
U_LONG leap_monthofleap;	/* in the month of a leap */
U_LONG leap_dayofleap;		/* This is the day of the leap */
U_LONG leap_hoursfromleap;	/* only 2 hours from leap */
U_LONG leap_happened;		/* leap process saw the leap */

/*
 * Imported from the main module
 */
extern int debug;


static void	setnexttimeout	P((U_LONG));

/*
 * init_leap - initialize the leap module's data.
 */
void
init_leap()
{
	/*
	 * Zero the indicators.  Schedule an event for just after
	 * initialization so we can sort things out.
	 */
	leap_indicator = leap_warning = leap_mask = 0;
	leap_timer = 1<<EVENT_TIMEOUT;
	leapbits = 0;

	leap_processcalls = leap_notclose = 0;
	leap_monthofleap = leap_dayofleap = 0;
	leap_hoursfromleap = leap_happened = 0;
}


/*
 * leap_process - process a leap event expiry and/or a system time step
 */
void
leap_process()
{
	U_LONG leapnext;
	U_LONG leaplast;
	l_fp ts;
	u_char bits;
	extern u_char sys_leap;

	leap_processcalls++;
	get_systime(&ts);
	calleapwhen(ts.l_ui, &leaplast, &leapnext);

	/*
	 * Figure out what to do based on how LONG to the next leap.
	 */
	if (leapnext > OKAYTOSETWARNING) {
		if (leaplast < ONEMINUTE) {
			/*
			 * The golden moment!  See if there's anything
			 * to do.
			 */
			leap_happened++;
			bits = 0;
			leap_mask = 0;
			if (leap_indicator != 0)
				bits = leap_indicator;
			else if (leapbits != 0)
				bits = leapbits;
			
			if (bits != 0 && !pll_control) {
				l_fp tmp;

				/*
				 * Step the clock 1 second in the proper
				 * direction.
				 */
				if (bits == LEAP_DELSECOND)
					tmp.l_i = 1;
				else
					tmp.l_i = -1;
				tmp.l_uf = 0;

				step_systime(&tmp);
#ifdef SLEWALWAYS
				syslog(LOG_NOTICE,
			"leap second occured, slewed time %s 1 second",
				    tmp.l_i > 0 ? "forward" : "back");
#else
				syslog(LOG_NOTICE,
			"leap second occured, stepped time %s 1 second",
				    tmp.l_i > 0 ? "forward" : "back");
#endif
			}
		} else {
			leap_notclose++;
		}
		leap_warning = 0;
	} else {
		if (leapnext > DAYBEFORE)
			leap_monthofleap++;
		else if (leapnext > TWOHOURSBEFORE)
			leap_dayofleap++;
		else
			leap_hoursfromleap++;
	}

	if (leapnext > DAYBEFORE) {
		leap_indicator = 0;
		leapbits = 0;
		/*
		 * Berkeley's setitimer call does result in alarm
		 * signal drift despite rumours to the contrary.
		 * Schedule an event no more than 24 hours into
		 * the future to allow the event time to be
		 * recomputed.
		 */
		if ((leapnext - DAYBEFORE) >= DAYBEFORE)
			setnexttimeout((U_LONG)DAYBEFORE);
		else
			setnexttimeout(leapnext - DAYBEFORE);
		return;
	}

	/*
	 * Here we're in the day of the leap.  Set the leap indicator
	 * bits from the warning, if necessary.
	 */
	if (leap_indicator == 0 && leap_warning != 0)
		leap_indicator = leap_warning;
	leap_mask = LEAP_NOTINSYNC;
	if (leapnext > TWOHOURSBEFORE) {
		leapbits = 0;
		setnexttimeout(leapnext - TWOHOURSBEFORE);
		return;
	}

	/*
	 * Here we're in the final 2 hours.  If sys_leap is set, set
	 * leapbits to it.
	 */
	if (sys_leap == LEAP_ADDSECOND || sys_leap == LEAP_DELSECOND)
		leapbits = sys_leap;
	setnexttimeout((leapnext > FIVEMINUTES) ? FIVEMINUTES : leapnext);
}


/*
 * setnexttimeout - set the next leap alarm
 */
static void
setnexttimeout(secs)
	U_LONG secs;
{
	/*
	 * We try to aim the time out at between 1 and 1+(1<<EVENT_TIMEOUT)
	 * seconds after the desired time.
	 */
	leap_timer = (secs + 1 + (1<<EVENT_TIMEOUT) + current_time)
	    & ~((1<<EVENT_TIMEOUT)-1);
}


/*
 * leap_setleap - set leap_indicator and/or leap_warning.  Return failure
 *		  if we don't want to do it.
 */
int
leap_setleap(indicator, warning)
	int indicator;
	int warning;
{
	U_LONG leapnext;
	U_LONG leaplast;
	l_fp ts;
	int i;

	get_systime(&ts);
	calleapwhen(ts.l_ui, &leaplast, &leapnext);

	i = 0;
	if (warning != ~0)
		if (leapnext > OKAYTOSETWARNING)
			i = 1;

	if (indicator != ~0)
		if (leapnext > DAYBEFORE)
			i = 1;
	
	if (i) {
		syslog(LOG_ERR,
		    "attempt to set leap bits at unlikely time of month");
		return 0;
	}

	if (warning != ~0)
		leap_warning = warning;

	if (indicator != ~0) {
		if (indicator == LEAP_NOWARNING) {
			leap_warning = LEAP_NOWARNING;
		}
		leap_indicator = indicator;
	}
	return 1;
}

/*
 * leap_actual
 *
 * calculate leap value - pass arg through of no local
 * configuration. Otherwise ise local configuration
 * (only used to cope with broken time servers and
 * broken refclocks)
 *
 * Mapping of leap_indicator:
 *	LEAP_NOWARNING
 *		pass peer value to sys_leap - usual operation
 *	LEAP_ADD/DEL_SECOND
 *		pass  LEAP_ADD/DEL_SECOND to sys_leap
 *	LEAP_NOTINSYNC
 *		pass LEAP_NOWARNING to sys_leap - effectively ignores leap
 */
/* there seems to be a bug in the IRIX 4 compiler which prevents
   u_char from beeing used in prototyped functions
   AIX also suffers from this.
   So give up and define it terms of int.
*/
int
leap_actual(l)
	int l ;
{
	if (leap_indicator != LEAP_NOWARNING) {
		if (leap_indicator == LEAP_NOTINSYNC)
			return LEAP_NOWARNING;
		else
			return leap_indicator;
	} else {
		return l;
	}
}

