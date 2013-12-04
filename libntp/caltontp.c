/*
 * caltontp - convert a date to an NTP time
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#include "ntp_assert.h"

/*
 * Juergen Perlinger, 2008-11-12
 * Add support for full calendar calculatios. If the day-of-year is provided
 * (that is, not zero) it will be used instead of month and day-of-month;
 * otherwise a full turn through the calendar calculations will be taken.
 *
 * I know that Harlan Stenn likes to see assertions in production code, and I
 * agree there, but it would be a tricky thing here. The algorithm is quite
 * capable of producing sensible answers even to seemingly weird inputs: the
 * date <any year here>-03-00, the 0.th March of the year, will be automtically
 * treated as the last day of February, no matter whether the year is a leap
 * year or not. So adding constraints is merely for the benefit of the callers,
 * because the only thing we can check for consistency is our input, produced
 * by somebody else.
 *
 * BTW: A total roundtrip using 'caljulian' would be a quite shaky thing:
 * Because of the truncation of the NTP time stamp to 32 bits and the epoch
 * unfolding around the current time done by 'caljulian' the roundtrip does
 * *not* necessarily reproduce the input, especially if the time spec is more
 * than 68 years off from the current time...
 */
u_long
caltontp(
	const struct calendar *jt
	)
{
	ntp_u_int32_t days;	/* full days in NTP epoch */
	ntp_u_int32_t years;	/* complete ACE years before date */
	ntp_u_int32_t month;	/* adjusted month for calendar */
	
	NTP_INSIST(jt != NULL);

	NTP_REQUIRE(jt->month <= 13);	/* permit month 0..13! */
	NTP_REQUIRE(jt->monthday <= 32);
	NTP_REQUIRE(jt->yearday <= 366);
	NTP_REQUIRE(jt->hour <= 24);
	NTP_REQUIRE(jt->minute <= MINSPERHR);
	NTP_REQUIRE(jt->second <= SECSPERMIN);

	/*
	 * First convert the date to fully elapsed days since NTP epoch. The
	 * expressions used here give us initially days since 0001-01-01, the
	 * beginning of the christian era in the proleptic gregorian calendar;
	 * they are rebased on-the-fly into days since beginning of the NTP
	 * epoch, 1900-01-01.
	 */
	if (jt->yearday) {
		/*
		 * Assume that the day-of-year contains a useable value and
		 * avoid all calculations involving month and day-of-month.
		 */
		years = jt->year - 1;
		days  = years * DAYSPERYEAR	/* days in previous years */
		      + years / 4		/* plus prior years's leap days */
		      - years / 100		/* minus leapless century years */
		      + years / 400		/* plus leapful Gregorian yrs */
		      + jt->yearday		/* days this year */
		      - DAY_NTP_STARTS;		/* rebase to NTP epoch */
	} else {
		/*
		 * The following code is according to the excellent book
		 * 'Calendrical Calculations' by Nachum Dershowitz and Edward
		 * Reingold. It does a full calendar evaluation, using one of
		 * the alternate algorithms: Shift to a hypothetical year
		 * starting on the previous march,1st; merge years, month and
		 * days; undo the the 9 month shift (which is 306 days). The
		 * advantage is that we do NOT need to now whether a year is a
		 * leap year or not, because the leap day is the LAST day of
		 * the year.
		 */
		month  = (ntp_u_int32_t)jt->month + 9;
		years  = jt->year - 1 + month / 12;
		month %= 12;
		days   = years * DAYSPERYEAR	/* days in previous years */
		       + years / 4		/* plus prior years's leap days */
		       - years / 100		/* minus leapless century years */
		       + years / 400		/* plus leapful Gregorian yrs */
		       + (month * 153 + 2) / 5	/* plus days before month */
		       + jt->monthday		/* plus day-of-month */
		       - 306			/* minus 9 months */
		       - DAY_NTP_STARTS;	/* rebase to NTP epoch */
	}

	/*
	 * Do the obvious: Merge everything together, making sure integer
	 * promotion doesn't play dirty tricks on us; there is probably some
	 * redundancy in the casts, but this drives it home with force. All
	 * arithmetic is done modulo 2**32, because the result is truncated
	 * anyway.
	 */
	return               days       * SECSPERDAY
	    + (ntp_u_int32_t)jt->hour   * MINSPERHR*SECSPERMIN
	    + (ntp_u_int32_t)jt->minute * SECSPERMIN
	    + (ntp_u_int32_t)jt->second;
}
