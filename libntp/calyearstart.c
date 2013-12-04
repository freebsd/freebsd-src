/*
 * calyearstart - determine the NTP time at midnight of January 1 in
 *		  the year of the given date.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#include "ntp_assert.h"

/*
 * Juergen Perlinger, 2008-11-12
 * Use the result of 'caljulian' to get the delta from the time stamp to the
 * beginning of the year. Do not make a second trip through 'caltontp' after
 * fixing the date, apart for invariant tests.
 */
u_long
calyearstart(u_long ntp_time)
{
	struct calendar jt;
	ntp_u_int32_t   delta;

	caljulian(ntp_time,&jt);
	
	/*
	* Now we have days since yearstart (unity-based) and the time in that
	* day. Simply merge these together to seconds and subtract that from
	* input time. That's faster than going through the calendar stuff
	* again...
	*/
	delta = (ntp_u_int32_t)jt.yearday * SECSPERDAY
	      + (ntp_u_int32_t)jt.hour    * MINSPERHR * SECSPERMIN
	      + (ntp_u_int32_t)jt.minute  * SECSPERMIN
	      + (ntp_u_int32_t)jt.second
	      - SECSPERDAY;	/* yearday is unity-based... */

#   if ISC_CHECK_INVARIANT
	/*
	 * check that this computes properly: do a roundtrip! That's the only
	 * sensible test here, but it's a rather expensive invariant...
	 */  
	jt.yearday  = 0;
	jt.month    = 1;
	jt.monthday = 1;
	jt.hour     = 0;
	jt.minute   = 0;
	jt.second   = 0;
	NTP_INVARIANT((ntp_u_int32_t)(caltontp(&jt) + delta) == (ntp_u_int32_t)ntp_time);
#   endif

	/* The NTP time stamps (l_fp) count seconds unsigned mod 2**32, so we
	 * have to calculate this in the proper way!
	 */
	return (ntp_u_int32_t)(ntp_time - delta);
}
