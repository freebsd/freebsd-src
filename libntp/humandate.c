/*
 * humandate - convert an NTP (or the current) time to something readable
 */
#include <stdio.h>
#include "ntp_fp.h"
#include "ntp_unixtime.h"	/* includes <sys/time.h> and <time.h> */
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

extern const char *months[];	/* prettydate.c */

/* This is used in msyslog.c; we don't want to clutter up the log with
   the year and day of the week, etc.; just the minimal date and time.  */

char *
humanlogtime(void)
{
	char *		bp;
	time_t		cursec;
	struct tm *	tm;
	
	cursec = time(NULL);
	tm = localtime(&cursec);
	if (!tm)
		return "-- --- --:--:--";

	LIB_GETBUF(bp);
	
	snprintf(bp, LIB_BUFLENGTH, "%2d %s %02d:%02d:%02d",
		 tm->tm_mday, months[tm->tm_mon],
		 tm->tm_hour, tm->tm_min, tm->tm_sec);
		
	return bp;
}
