/*
 * humandate - convert an NTP (or the current) time to something readable
 */
#include <stdio.h>
#include "ntp_fp.h"
#include "ntp_unixtime.h"	/* includes <sys/time.h> and <time.h> */
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
humandate(
	u_long ntptime
	)
{
	char *bp;
	struct tm *tm;

	tm = ntp2unix_tm(ntptime, 1);

	if (!tm)
		return "--- --- -- ---- --:--:--";

	LIB_GETBUF(bp);
	
	(void) sprintf(bp, "%s, %s %2d %4d %2d:%02d:%02d",
		       days[tm->tm_wday], months[tm->tm_mon], tm->tm_mday,
		       1900+tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
	
	return bp;
}


/* This is used in msyslog.c; we don't want to clutter up the log with
   the year and day of the week, etc.; just the minimal date and time.  */

char *
humanlogtime(void)
{
	char *bp;
	time_t cursec = time((time_t *) 0);
	struct tm *tm;
	
	tm = localtime(&cursec);
	if (!tm)
		return "-- --- --:--:--";

	LIB_GETBUF(bp);
	
	(void) sprintf(bp, "%2d %s %02d:%02d:%02d",
		       tm->tm_mday, months[tm->tm_mon],
		       tm->tm_hour, tm->tm_min, tm->tm_sec);
		
	return bp;
}
