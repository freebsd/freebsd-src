/*
 * caljulian - determine the Julian date from an NTP time.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

/*
 * calmonthtab - days-in-the-month table
 */
static u_short calmonthtab[11] = {
	JAN,
	FEB,
	MAR,
	APR,
	MAY,
	JUN,
	JUL,
	AUG,
	SEP,
	OCT,
	NOV
};

void
caljulian(
	u_long		  		ntptime,
	register struct calendar	*jt
	)
{
	u_long ntp_day;
	u_long minutes;
	/*
	 * Absolute, zero-adjusted Christian era day, starting from the
	 * mythical day 12/1/1 BC
	 */
	u_long acez_day;

	u_long d400;				 /* Days into a Gregorian cycle */
	u_long d100;				 /* Days into a normal century */
	u_long d4;					 /* Days into a 4-year cycle */
	u_long n400;				 /* # of Gregorian cycles */
	u_long n100;				 /* # of normal centuries */
	u_long n4;					 /* # of 4-year cycles */
	u_long n1;					 /* # of years into a leap year */
						 /*   cycle */

	/*
	 * Do the easy stuff first: take care of hh:mm:ss, ignoring leap
	 * seconds
	 */
	jt->second = (u_char)(ntptime % SECSPERMIN);
	minutes    = ntptime / SECSPERMIN;
	jt->minute = (u_char)(minutes % MINSPERHR);
	jt->hour   = (u_char)((minutes / MINSPERHR) % HRSPERDAY);

	/*
	 * Find the day past 1900/01/01 00:00 UTC
	 */
	ntp_day = ntptime / SECSPERDAY;
	acez_day = DAY_NTP_STARTS + ntp_day - 1;
	n400	 = acez_day/GREGORIAN_CYCLE_DAYS;
	d400	 = acez_day%GREGORIAN_CYCLE_DAYS;
	n100	 = d400 / GREGORIAN_NORMAL_CENTURY_DAYS;
	d100	 = d400 % GREGORIAN_NORMAL_CENTURY_DAYS;
	n4		 = d100 / GREGORIAN_NORMAL_LEAP_CYCLE_DAYS;
	d4		 = d100 % GREGORIAN_NORMAL_LEAP_CYCLE_DAYS;
	n1		 = d4 / DAYSPERYEAR;

	/*
	 * Calculate the year and year-of-day
	 */
	jt->yearday = (u_short)(1 + d4%DAYSPERYEAR);
	jt->year	= (u_short)(400*n400 + 100*n100 + n4*4 + n1);

	if (n100 == 4 || n1 == 4)
	{
	/*
	 * If the cycle year ever comes out to 4, it must be December 31st
	 * of a leap year.
	 */
	jt->month	 = 12;
	jt->monthday = 31;
	jt->yearday  = 366;
	}
	else
	{
	/*
	 * Else, search forwards through the months to get the right month
	 * and date.
	 */
	int monthday;

	jt->year++;
	monthday = jt->yearday;

	for (jt->month=0;jt->month<11; jt->month++)
	{
		int t;

		t = monthday - calmonthtab[jt->month];
		if (jt->month == 1 && is_leapyear(jt->year))
		t--;

		if (t > 0)
		monthday = t;
		else
		break;
	}
	jt->month++;
	jt->monthday = monthday;
	}
}
