/* prettydate.c,v 3.1 1993/07/06 01:08:42 jbj Exp
 * prettydate - convert a time stamp to something readable
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

#ifdef NTP_POSIX_SOURCE
#include <time.h>
#endif

char *
prettydate(ts)
	l_fp *ts;
{
	char *bp;
	struct tm *tm;
	U_LONG sec;
	U_LONG msec;
	static char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	static char *days[] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};

	LIB_GETBUF(bp);
	
	sec = ts->l_ui - JAN_1970;
	msec = ts->l_uf / 4294967;	/* fract / (2**32/1000) */

	tm = localtime((LONG *)&sec);

	(void) sprintf(bp, "%08x.%08x  %s, %s %2d %4d %2d:%02d:%02d.%03d",
	    ts->l_ui, ts->l_uf, days[tm->tm_wday], months[tm->tm_mon],
	    tm->tm_mday, 1900+tm->tm_year, tm->tm_hour, tm->tm_min,
	    tm->tm_sec, msec);
	
	return bp;
}
