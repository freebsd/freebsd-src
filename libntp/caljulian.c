/*
 * caljulian - determine the Julian date from an NTP time.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#include "ntp_fp.h"
#include "ntp_unixtime.h"

#if !(defined(ISC_CHECK_ALL) || defined(ISC_CHECK_NONE) || \
      defined(ISC_CHECK_ENSURE) || defined(ISC_CHECK_INSIST) || \
      defined(ISC_CHECK_INVARIANT))
# define ISC_CHECK_ALL
#endif

#include "ntp_assert.h"

#if 1

/* Updated 2008-11-10 Juergen Perlinger <juergen.perlinger@t-online.de>
 *
 * Make the conversion 2038-proof with proper NTP epoch unfolding and extended
 * precision calculations. Though we should really get a 'time_t' with more
 * than 32 bits at least until 2037, because the unfolding cannot work after
 * the wrap of the 32-bit 'time_t'.
 */

void
caljulian(
	u_long		  		ntptime,
	register struct calendar	*jt
	)
{
	u_long  saved_time = ntptime;
	u_long  ntp_day; /* days (since christian era or in year) */ 
	u_long  n400;    /* # of Gregorian cycles */
	u_long  n100;    /* # of normal centuries */
	u_long  n4;      /* # of 4-year cycles */
	u_long  n1;      /* # of years into a leap year cycle */
	u_long  sclday;  /* scaled days for month conversion */
	int     leaps;   /* # of leaps days in year */
	time_t  now;     /* current system time */
	u_int32 tmplo;   /* double precision tmp value / lo part */
	int32   tmphi;   /* double precision tmp value / hi part */

	NTP_INSIST(NULL != jt);

	/*
	 * First we have to unfold the ntp time stamp around the current time
	 * to make sure we are in the right epoch. Also we we do *NOT* fold
	 * before the begin of the first NTP epoch, so we WILL have a
	 * non-negative time stamp afterwards. Though at the time of this
	 * writing (2008 A.D.) it would be really strange to have systems
	 * running with clock set to he 1960's or before...
	 *
	 * But's important to use a 32 bit max signed value -- LONG_MAX is 64
	 * bit on a 64-bit system, and it will give wrong results.
	 */
	now   = time(NULL);
	tmplo = (u_int32)now;
#if ( SIZEOF_TIME_T > 4 )
	tmphi = (int32)(now >> 16 >> 16);
#else
	/*
	 * Get the correct sign extension in the high part. 
	 * (now >> 32) may not work correctly on every 32 bit 
	 * system, e.g. it yields garbage under Win32/VC6.
	 */
    tmphi = (int32)(now >> 31);
#endif
	
	M_ADD(tmphi, tmplo, 0, ((1UL << 31)-1)); /* 32-bit max signed */
	M_ADD(tmphi, tmplo, 0, JAN_1970);
	if ((ntptime > tmplo) && (tmphi > 0))
		--tmphi;
	tmplo = ntptime;
	
	/*
	 * Now split into days and seconds-of-day, using the fact that
	 * SECSPERDAY (86400) == 675 * 128; we can get roughly 17000 years of
	 * time scale, using only 32-bit calculations. Some magic numbers here,
	 * sorry for that. (This could be streamlined for 64 bit machines, but
	 * is worth the trouble?)
	 */
	ntptime  = tmplo & 127;	/* save remainder bits */
	tmplo    = (tmplo >> 7) | (tmphi << 25);
	ntp_day  =  (u_int32)tmplo / 675;
	ntptime += ((u_int32)tmplo % 675) << 7;

	/* some checks for the algorithm 
	 * There's some 64-bit trouble out there: the original NTP time stamp
	 * had only 32 bits, so our calculation invariant only holds in 32 bits!
	 */
	NTP_ENSURE(ntptime < SECSPERDAY);
	NTP_INVARIANT((u_int32)(ntptime + ntp_day * SECSPERDAY) == (u_int32)saved_time);

	/*
	 * Do the easy stuff first: take care of hh:mm:ss, ignoring leap
	 * seconds
	 */
	jt->second = (u_char)(ntptime % SECSPERMIN);
	ntptime   /= SECSPERMIN;
	jt->minute = (u_char)(ntptime % MINSPERHR);
	ntptime   /= MINSPERHR;
	jt->hour   = (u_char)(ntptime);

	/* check time invariants */
	NTP_ENSURE(jt->second < SECSPERMIN);
	NTP_ENSURE(jt->minute < MINSPERHR);
	NTP_ENSURE(jt->hour   < HRSPERDAY);

	/*
	 * Find the day past 1900/01/01 00:00 UTC
	 */
	ntp_day += DAY_NTP_STARTS - 1;	/* convert to days in CE */
	n400	 = ntp_day / GREGORIAN_CYCLE_DAYS; /* split off cycles */
	ntp_day %= GREGORIAN_CYCLE_DAYS;
	n100	 = ntp_day / GREGORIAN_NORMAL_CENTURY_DAYS;
	ntp_day %= GREGORIAN_NORMAL_CENTURY_DAYS;
	n4	 = ntp_day / GREGORIAN_NORMAL_LEAP_CYCLE_DAYS;
	ntp_day %= GREGORIAN_NORMAL_LEAP_CYCLE_DAYS;
	n1	 = ntp_day / DAYSPERYEAR;
	ntp_day %= DAYSPERYEAR; /* now zero-based day-of-year */

	NTP_ENSURE(ntp_day < 366);

	/*
	 * Calculate the year and day-of-year
	 */
	jt->year = (u_short)(400*n400 + 100*n100 + 4*n4 + n1);

	if ((n100 | n1) > 3) {
		/*
		 * If the cycle year ever comes out to 4, it must be December
		 * 31st of a leap year.
		 */
		jt->month    = 12;
		jt->monthday = 31;
		jt->yearday  = 366;
	} else {
		/*
		 * The following code is according to the excellent book
		 * 'Calendrical Calculations' by Nachum Dershowitz and Edward
		 * Reingold. It converts the day-of-year into month and
		 * day-of-month, using a linear transformation with integer
		 * truncation. Magic numbers again, but they will not be used
		 * anywhere else.
		 */
		sclday = ntp_day * 7 + 217;
		leaps  = ((n1 == 3) && ((n4 != 24) || (n100 == 3))) ? 1 : 0;
		if (ntp_day >= (u_long)(JAN + FEB + leaps))
			sclday += (2 - leaps) * 7;
		++jt->year;
		jt->month    = (u_char)(sclday / 214);
		jt->monthday = (u_char)((sclday % 214) / 7 + 1);
		jt->yearday  = (u_short)(1 + ntp_day);
	}

	/* check date invariants */
	NTP_ENSURE(1 <= jt->month    && jt->month    <=  12); 
	NTP_ENSURE(1 <= jt->monthday && jt->monthday <=  31);
	NTP_ENSURE(1 <= jt->yearday  && jt->yearday  <= 366);
}

#else

/* Updated 2003-12-30 TMa

   Uses common code with the *prettydate functions to convert an ntp
   seconds count into a calendar date.
   Will handle ntp epoch wraparound as long as the underlying os/library 
   does so for the unix epoch, i.e. works after 2038.
*/

void
caljulian(
	u_long		  		ntptime,
	register struct calendar	*jt
	)
{
	struct tm *tm;
	NTP_REQUIRE(jt != NULL);

	tm = ntp2unix_tm(ntptime, 0);
	NTP_INSIST(tm != NULL);

	jt->hour = (u_char) tm->tm_hour;
	jt->minute = (u_char) tm->tm_min;
	jt->month = (u_char) (tm->tm_mon + 1);
	jt->monthday = (u_char) tm->tm_mday;
	jt->second = (u_char) tm->tm_sec;
	jt->year = (u_short) (tm->tm_year + 1900);
	jt->yearday = (u_short) (tm->tm_yday + 1);  /* Assumes tm_yday starts with day 0! */
}
#endif
