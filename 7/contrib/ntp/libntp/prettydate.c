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

/* Helper function to handle possible wraparound of the ntp epoch.

   Works by assuming that the localtime/gmtime library functions 
   have been updated so that they work
*/

#define MAX_EPOCH_NR 1000

struct tm *
ntp2unix_tm(
	u_long ntp, int local
	)
{
	time_t t, curr;
	struct tm *tm;
	int curr_year, epoch_nr;

	/* First get the current year: */
	curr = time(NULL);
	tm = local ? localtime(&curr) : gmtime(&curr);
	if (!tm) return NULL;

	curr_year = 1900 + tm->tm_year;

	/* Convert the ntp timestamp to a unix utc seconds count: */
	t = (time_t) ntp - JAN_1970;

	/* Check that the ntp timestamp is not before a 136 year window centered
	   around the current year:

	   Failsafe in case of an infinite loop:
       Allow up to 1000 epochs of 136 years each!
	*/
    for (epoch_nr = 0; epoch_nr < MAX_EPOCH_NR; epoch_nr++) {
		tm = local ? localtime(&t) : gmtime(&t);

#if SIZEOF_TIME_T < 4
# include "Bletch: sizeof(time_t) < 4!"
#endif

#if SIZEOF_TIME_T == 4
		/* If 32 bits, then year is 1970-2038, so no sense looking */
		epoch_nr = MAX_EPOCH_NR;
#else	/* SIZEOF_TIME_T > 4 */
		/* Check that the resulting year is in the correct epoch: */
		if (1900 + tm->tm_year > curr_year - 68) break;

		/* Epoch wraparound: Add 2^32 seconds! */
		t += (time_t) 65536 << 16;
#endif /* SIZEOF_TIME_T > 4 */
	}
	return tm;
}

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
	
	sec = ts->l_ui;
	msec = ts->l_uf / 4294967;	/* fract / (2 ** 32 / 1000) */

	tm = ntp2unix_tm(sec, 1);
	if (!tm) {
		(void) sprintf(bp, "%08lx.%08lx  --- --- -- ---- --:--:--",
		       (u_long)ts->l_ui, (u_long)ts->l_uf);
	}
	else {
		(void) sprintf(bp, "%08lx.%08lx  %s, %s %2d %4d %2d:%02d:%02d.%03lu",
		       (u_long)ts->l_ui, (u_long)ts->l_uf, days[tm->tm_wday],
		       months[tm->tm_mon], tm->tm_mday, 1900 + tm->tm_year,
		       tm->tm_hour,tm->tm_min, tm->tm_sec, msec);
	}
	
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
	
	sec = ts->l_ui;
	msec = ts->l_uf / 4294967;	/* fract / (2 ** 32 / 1000) */

	tm = ntp2unix_tm(sec, 0);
	if (!tm) {
		(void) sprintf(bp, "%08lx.%08lx  --- --- -- ---- --:--:--",
		       (u_long)ts->l_ui, (u_long)ts->l_uf);
	}
	else {
		(void) sprintf(bp, "%08lx.%08lx  %s, %s %2d %4d %2d:%02d:%02d.%03lu",
		       (u_long)ts->l_ui, (u_long)ts->l_uf, days[tm->tm_wday],
		       months[tm->tm_mon], tm->tm_mday, 1900 + tm->tm_year,
		       tm->tm_hour,tm->tm_min, tm->tm_sec, msec);
	}

	return bp;
}
