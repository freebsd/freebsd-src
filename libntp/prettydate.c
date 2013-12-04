/*
 * prettydate - convert a time stamp to something readable
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp_unixtime.h"	/* includes <sys/time.h> */
#include "lib_strbuf.h"
#include "ntp_stdlib.h"
#include "ntp_assert.h"

static char *common_prettydate(l_fp *, int);

const char *months[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *days[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/* Helper function to handle possible wraparound of the ntp epoch.

   Works by periodic extension of the ntp time stamp in the NTP epoch.  If the
   'time_t' is 32 bit, use solar cycle warping to get the value in a suitable
   range. Also uses solar cycle warping to work around really buggy
   implementations of 'gmtime()' / 'localtime()' that cannot work with a
   negative time value, that is, times before 1970-01-01. (MSVCRT...)

   Apart from that we're assuming that the localtime/gmtime library functions
   have been updated so that they work...
*/


/* solar cycle in secs, unsigned secs and years. And the cycle limits.
**
** And an explanation. The julian calendar repeats ever 28 years, because it's
** the LCM of 7 and 4, the week and leap year cycles. This is called a 'solar
** cycle'. The gregorian calendar does the same as long as no centennial year
** (divisible by 100, but not 400) goes in the way. So between 1901 and 2099
** (inclusive) we can warp time stamps by 28 years to make them suitable for
** localtime() and gmtime() if we have trouble. Of course this will play
** hubbubb with the DST zone switches, so we should do it only if necessary;
** but as we NEED a proper conversion to dates via gmtime() we should try to
** cope with as many idiosyncrasies as possible.
*/
#define SOLAR_CYCLE_SECS   0x34AADC80UL	/* 7*1461*86400*/
#define SOLAR_CYCLE_YEARS  28
#define MINFOLD -3
#define MAXFOLD  3

struct tm *
ntp2unix_tm(
	u_long ntp, int local
	)
{
	struct tm *tm;
	int32      folds = 0;
	time_t     t     = time(NULL);
	u_int32    dwlo  = (int32)t; /* might expand for SIZEOF_TIME_T < 4 */
#if ( SIZEOF_TIME_T > 4 )
	int32      dwhi  = (int32)(t >> 16 >> 16);/* double shift: avoid warnings */
#else
	/*
	 * Get the correct sign extension in the high part. 
	 * (now >> 32) may not work correctly on every 32 bit 
	 * system, e.g. it yields garbage under Win32/VC6.
	 */
    int32		dwhi = (int32)(t >> 31);
#endif
	
	/* Shift NTP to UN*X epoch, then unfold around currrent time. It's
	 * important to use a 32 bit max signed value -- LONG_MAX is 64 bit on
	 * a 64-bit system, and it will give wrong results.
	 */
	M_ADD(dwhi, dwlo, 0, ((1UL << 31)-1)); /* 32-bit max signed */
	if ((ntp -= JAN_1970) > dwlo)
		--dwhi;
	dwlo = ntp;
 
#   if SIZEOF_TIME_T < 4
#	error sizeof(time_t) < 4 -- this will not work!
#   elif SIZEOF_TIME_T == 4

	/*
	** If the result will not fit into a 'time_t' we have to warp solar
	** cycles. That's implemented by looped addition / subtraction with
	** M_ADD and M_SUB to avoid implicit 64 bit operations, especially
	** division. As he number of warps is rather limited there's no big
	** performance loss here.
	**
	** note: unless the high word doesn't match the sign-extended low word,
	** the combination will not fit into time_t. That's what we use for
	** loop control here...
	*/
	while (dwhi != ((int32)dwlo >> 31)) {
		if (dwhi < 0 && --folds >= MINFOLD)
			M_ADD(dwhi, dwlo, 0, SOLAR_CYCLE_SECS);
		else if (dwhi >= 0 && ++folds <= MAXFOLD)
			M_SUB(dwhi, dwlo, 0, SOLAR_CYCLE_SECS);
		else
			return NULL;
	}

#   else

	/* everything fine -- no reduction needed for the next thousand years */

#   endif

	/* combine hi/lo to make time stamp */
	t = ((time_t)dwhi << 16 << 16) | dwlo;	/* double shift: avoid warnings */

#   ifdef _MSC_VER	/* make this an autoconf option? */

	/*
	** The MSDN says that the (Microsoft) Windoze versions of 'gmtime()'
	** and 'localtime()' will bark on time stamps < 0. Better to fix it
	** immediately.
	*/
	while (t < 0) { 
		if (--folds < MINFOLD)
			return NULL;
		t += SOLAR_CYCLE_SECS;
	}
			
#   endif /* Microsoft specific */

	/* 't' should be a suitable value by now. Just go ahead. */
	while ( (tm = (*(local ? localtime : gmtime))(&t)) == 0)
		/* seems there are some other pathological implementations of
		** 'gmtime()' and 'localtime()' somewhere out there. No matter
		** if we have 32-bit or 64-bit 'time_t', try to fix this by
		** solar cycle warping again...
		*/
		if (t < 0) {
			if (--folds < MINFOLD)
				return NULL;
			t += SOLAR_CYCLE_SECS;
		} else {
			if ((++folds > MAXFOLD) || ((t -= SOLAR_CYCLE_SECS) < 0))
				return NULL; /* That's truely pathological! */
		}
	/* 'tm' surely not NULL here... */
	NTP_INSIST(tm != NULL);
	if (folds != 0) {
		tm->tm_year += folds * SOLAR_CYCLE_YEARS;
		if (tm->tm_year <= 0 || tm->tm_year >= 200)
			return NULL;	/* left warp range... can't help here! */
	}
	return tm;
}


static char *
common_prettydate(
	l_fp *ts,
	int local
	)
{
	char *bp;
	struct tm *tm;
	u_long sec;
	u_long msec;

	LIB_GETBUF(bp);
	
	sec = ts->l_ui;
	msec = ts->l_uf / 4294967;	/* fract / (2 ** 32 / 1000) */

	tm = ntp2unix_tm(sec, local);
	if (!tm)
		snprintf(bp, LIB_BUFLENGTH,
			 "%08lx.%08lx  --- --- -- ---- --:--:--",
			 (u_long)ts->l_ui, (u_long)ts->l_uf);
	else
		snprintf(bp, LIB_BUFLENGTH,
			 "%08lx.%08lx  %s, %s %2d %4d %2d:%02d:%02d.%03lu",
			 (u_long)ts->l_ui, (u_long)ts->l_uf,
			 days[tm->tm_wday], months[tm->tm_mon],
			 tm->tm_mday, 1900 + tm->tm_year, tm->tm_hour,
			 tm->tm_min, tm->tm_sec, msec);
	
	return bp;
}


char *
prettydate(
	l_fp *ts
	)
{
	return common_prettydate(ts, 1);
}


char *
gmprettydate(
	l_fp *ts
	)
{
	return common_prettydate(ts, 0);
}
