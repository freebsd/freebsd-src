/*
 * prettydate - convert a time stamp to something readable
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp_unixtime.h"	/* includes <sys/time.h> */
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

static const char *months[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *days[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

char *
prettydate(
	l_fp *ts
	)
{
	char *bp;
	struct tm *tm;
	time_t sec;
	u_long msec;

	LIB_GETBUF(bp);
	
	sec = ts->l_ui - JAN_1970;
	msec = ts->l_uf / 4294967;	/* fract / (2 ** 32 / 1000) */

	tm = localtime(&sec);

	(void) sprintf(bp, "%08lx.%08lx  %s, %s %2d %4d %2d:%02d:%02d.%03lu",
		       (u_long)ts->l_ui, (u_long)ts->l_uf, days[tm->tm_wday],
		       months[tm->tm_mon], tm->tm_mday, 1900 + tm->tm_year,
		       tm->tm_hour,tm->tm_min, tm->tm_sec, msec);
	
	return bp;
}

char *
gmprettydate(
	l_fp *ts
	)
{
	char *bp;
	struct tm *tm;
	time_t sec;
	u_long msec;

	LIB_GETBUF(bp);
	
	sec = ts->l_ui - JAN_1970;
	msec = ts->l_uf / 4294967;	/* fract / (2 ** 32 / 1000) */

	tm = gmtime(&sec);

	(void) sprintf(bp, "%08lx.%08lx  %s, %s %2d %4d %2d:%02d:%02d.%03lu UTC",
		       (u_long)ts->l_ui, (u_long)ts->l_uf, days[tm->tm_wday],
		       months[tm->tm_mon], tm->tm_mday, 1900 + tm->tm_year,
		       tm->tm_hour,tm->tm_min, tm->tm_sec, msec);
	
	return bp;
}
