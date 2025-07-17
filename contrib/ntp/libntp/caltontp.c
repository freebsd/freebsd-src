/*
 * caltontp - convert a date to an NTP time
 */
#include <config.h>
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#include "ntp_assert.h"
#include "ntp_unixtime.h"

/*
 * Juergen Perlinger, 2008-11-12
 * Add support for full calendar calculations. If the day-of-year is provided
 * (that is, not zero) it will be used instead of month and day-of-month;
 * otherwise a full turn through the calendar calculations will be taken.
 *
 * I know that Harlan Stenn likes to see assertions in production code, and I
 * agree in general. But here we set 'errno' and try to do our best instead.
 * Also note that the bounds check is a bit sloppy: It permits off-by-one
 * on the input quantities. That permits some simple/naive adjustments to
 * be made before calling this function.
 *
 * Apart from that the calendar is perfectly capable of dealing with
 * off-scale input values!
 *
 * BTW: A total roundtrip using 'caljulian' would be a quite shaky thing:
 * Because of the truncation of the NTP time stamp to 32 bits and the epoch
 * unfolding around the current time done by 'caljulian' the roundtrip does
 * *not* necessarily reproduce the input, especially if the time spec is more
 * than 68 years off from the current time...
 */

uint32_t
caltontp(
	const struct calendar *jt
	)
{
	int32_t eraday;	/* CE Rata Die number	*/
	vint64  ntptime;/* resulting NTP time	*/

	if (NULL == jt) {
		errno = EINVAL;
		return 0;
	}

	if (   (jt->month > 13)	/* permit month 0..13! */
	    || (jt->monthday > 32)
	    || (jt->yearday > 366)
	    || (jt->hour > 24)
	    || (jt->minute > MINSPERHR)
	    || (jt->second > SECSPERMIN))
		errno = ERANGE;

	/*
	 * First convert the date to he corresponding RataDie
	 * number. If yearday is not zero, assume that it contains a
	 * useable value and avoid all calculations involving month
	 * and day-of-month. Do a full evaluation otherwise.
	 */
	if (jt->yearday)
		eraday = ntpcal_year_to_ystart(jt->year)
		       + jt->yearday - 1;
	else
		eraday = ntpcal_date_to_rd(jt);

	ntptime = ntpcal_dayjoin(eraday - DAY_NTP_STARTS,
				 ntpcal_etime_to_seconds(jt->hour, jt->minute,
							 jt->second));
	return ntptime.d_s.lo;
}
