/* humandate.c,v 3.1 1993/07/06 01:08:24 jbj Exp
 * humandate - convert an NTP (or the current) time to something readable
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

#ifdef NTP_POSIX_SOURCE
#include <time.h>
#endif

static char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static char *days[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

char *
humandate(ntptime)
	U_LONG ntptime;
{
	char *bp;
	struct tm *tm;
	U_LONG sec;

	LIB_GETBUF(bp);
	
	sec = ntptime - JAN_1970;
	tm = localtime((LONG *)&sec);

	(void) sprintf(bp, "%s, %s %2d %4d %2d:%02d:%02d",
	    days[tm->tm_wday], months[tm->tm_mon], tm->tm_mday,
	    1900+tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
	
	return bp;
}


/* This is used in msyslog.c; we don't want to clutter up the log with
   the year and day of the week, etc.; just the minimal date and time.  */

char *
humanlogtime()
{
	char *bp;
	time_t cursec = time((time_t *) 0);
	struct tm *tm = localtime(&cursec);
	
	LIB_GETBUF(bp);
	
	(void) sprintf(bp, "%2d %s %02d:%02d:%02d",
		tm->tm_mday, months[tm->tm_mon],
		tm->tm_hour, tm->tm_min, tm->tm_sec);
		
	return bp;
}
