/*	$NetBSD: timegm.c,v 1.3 2005/05/11 01:01:56 lukem Exp $	*/
/*	from	?	*/

#include "tnftp.h"

/*
 * UTC version of mktime(3)
 */

/*
 * This code is not portable, but works on most Unix-like systems.
 * If the local timezone has no summer time, using mktime(3) function
 * and adjusting offset would be usable (adjusting leap seconds
 * is still required, though), but the assumption is not always true.
 *
 * Anyway, no portable and correct implementation of UTC to time_t
 * conversion exists....
 */

static time_t
sub_mkgmt(struct tm *tm)
{
	int y, nleapdays;
	time_t t;
	/* days before the month */
	static const unsigned short moff[12] = {
		0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};

	/*
	 * XXX: This code assumes the given time to be normalized.
	 * Normalizing here is impossible in case the given time is a leap
	 * second but the local time library is ignorant of leap seconds.
	 */

	/* minimal sanity checking not to access outside of the array */
	if ((unsigned) tm->tm_mon >= 12)
		return (time_t) -1;
	if (tm->tm_year < EPOCH_YEAR - TM_YEAR_BASE)
		return (time_t) -1;

	y = tm->tm_year + TM_YEAR_BASE - (tm->tm_mon < 2);
	nleapdays = y / 4 - y / 100 + y / 400 -
	    ((EPOCH_YEAR-1) / 4 - (EPOCH_YEAR-1) / 100 + (EPOCH_YEAR-1) / 400);
	t = ((((time_t) (tm->tm_year - (EPOCH_YEAR - TM_YEAR_BASE)) * 365 +
			moff[tm->tm_mon] + tm->tm_mday - 1 + nleapdays) * 24 +
		tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;

	return (t < 0 ? (time_t) -1 : t);
}

time_t
timegm(struct tm *tm)
{
	time_t t, t2;
	struct tm *tm2;
	int sec;

	/* Do the first guess. */
	if ((t = sub_mkgmt(tm)) == (time_t) -1)
		return (time_t) -1;

	/* save value in case *tm is overwritten by gmtime() */
	sec = tm->tm_sec;

	tm2 = gmtime(&t);
	if ((t2 = sub_mkgmt(tm2)) == (time_t) -1)
		return (time_t) -1;

	if (t2 < t || tm2->tm_sec != sec) {
		/*
		 * Adjust for leap seconds.
		 *
		 *     real time_t time
		 *           |
		 *          tm
		 *         /	... (a) first sub_mkgmt() conversion
		 *       t
		 *       |
		 *      tm2
		 *     /	... (b) second sub_mkgmt() conversion
		 *   t2
		 *			--->time
		 */
		/*
		 * Do the second guess, assuming (a) and (b) are almost equal.
		 */
		t += t - t2;
		tm2 = gmtime(&t);

		/*
		 * Either (a) or (b), may include one or two extra
		 * leap seconds.  Try t, t + 2, t - 2, t + 1, and t - 1.
		 */
		if (tm2->tm_sec == sec
		    || (t += 2, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t -= 4, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t += 3, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t -= 2, tm2 = gmtime(&t), tm2->tm_sec == sec))
			;	/* found */
		else {
			/*
			 * Not found.
			 */
			if (sec >= 60)
				/*
				 * The given time is a leap second
				 * (sec 60 or 61), but the time library
				 * is ignorant of the leap second.
				 */
				;	/* treat sec 60 as 59,
					   sec 61 as 0 of the next minute */
			else
				/* The given time may not be normalized. */
				t++;	/* restore t */
		}
	}

	return (t < 0 ? (time_t) -1 : t);
}
