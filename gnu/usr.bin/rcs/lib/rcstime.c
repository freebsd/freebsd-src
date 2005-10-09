/* Convert between RCS time format and Posix and/or C formats.  */

/* Copyright 1992, 1993, 1994, 1995 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

#include "rcsbase.h"
#include "partime.h"
#include "maketime.h"

libId(rcstimeId, "$FreeBSD$")

static long zone_offset; /* seconds east of UTC, or TM_LOCAL_ZONE */
static int use_zone_offset; /* if zero, use UTC without zone indication */

/*
* Convert Unix time to RCS format.
* For compatibility with older versions of RCS,
* dates from 1900 through 1999 are stored without the leading "19".
*/
	void
time2date(unixtime,date)
	time_t unixtime;
	char date[datesize];
{
	register struct tm const *tm = time2tm(unixtime, RCSversion<VERSION(5));
	VOID sprintf(date,
#		if has_printf_dot
			"%.2d.%.2d.%.2d.%.2d.%.2d.%.2d",
#		else
			"%02d.%02d.%02d.%02d.%02d.%02d",
#		endif
		tm->tm_year  +  ((unsigned)tm->tm_year < 100 ? 0 : 1900),
		tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec
	);
}

/* Like str2time, except die if an error was found.  */
static time_t str2time_checked P((char const*,time_t,long));
	static time_t
str2time_checked(source, default_time, default_zone)
	char const *source;
	time_t default_time;
	long default_zone;
{
	time_t t = str2time(source, default_time, default_zone);
	if (t == -1)
		faterror("unknown date/time: %s", source);
	return t;
}

/*
* Parse a free-format date in SOURCE, convert it
* into RCS internal format, and store the result into TARGET.
*/
	void
str2date(source, target)
	char const *source;
	char target[datesize];
{
	time2date(
		str2time_checked(source, now(),
			use_zone_offset ? zone_offset
			: RCSversion<VERSION(5) ? TM_LOCAL_ZONE
			: 0
		),
		target
	);
}

/* Convert an RCS internal format date to time_t.  */
	time_t
date2time(source)
	char const source[datesize];
{
	char s[datesize + zonelenmax];
	return str2time_checked(date2str(source, s), (time_t)0, 0);
}


/* Set the time zone for date2str output.  */
	void
zone_set(s)
	char const *s;
{
	if ((use_zone_offset = *s)) {
		long zone;
		char const *zonetail = parzone(s, &zone);
		if (!zonetail || *zonetail)
			error("%s: not a known time zone", s);
		else
			zone_offset = zone;
	}
}


/*
* Format a user-readable form of the RCS format DATE into the buffer DATEBUF.
* Yield DATEBUF.
*/
	char const *
date2str(date, datebuf)
	char const date[datesize];
	char datebuf[datesize + zonelenmax];
{
	register char const *p = date;

	while (*p++ != '.')
		continue;
	if (!use_zone_offset)
	    VOID sprintf(datebuf,
		"19%.*s/%.2s/%.2s %.2s:%.2s:%s"
			+ (date[2]=='.' && VERSION(5)<=RCSversion  ?  0  :  2),
		(int)(p-date-1), date,
		p, p+3, p+6, p+9, p+12
	    );
	else {
	    struct tm t;
	    struct tm const *z;
	    int non_hour;
	    long zone;
	    char c;

	    t.tm_year = atoi(date) - (date[2]=='.' ? 0 : 1900);
	    t.tm_mon = atoi(p) - 1;
	    t.tm_mday = atoi(p+3);
	    t.tm_hour = atoi(p+6);
	    t.tm_min = atoi(p+9);
	    t.tm_sec = atoi(p+12);
	    t.tm_wday = -1;
	    zone = zone_offset;
	    if (zone == TM_LOCAL_ZONE) {
		time_t u = tm2time(&t, 0), d;
		z = localtime(&u);
		d = difftm(z, &t);
		zone  =  (time_t)-1 < 0 || d < -d  ?  d  :  -(long)-d;
	    } else {
		adjzone(&t, zone);
		z = &t;
	    }
	    c = '+';
	    if (zone < 0) {
		zone = -zone;
		c = '-';
	    }
	    VOID sprintf(datebuf,
#		if has_printf_dot
		    "%.2d-%.2d-%.2d %.2d:%.2d:%.2d%c%.2d",
#		else
		    "%02d-%02d-%02d %02d:%02d:%02d%c%02d",
#		endif
		z->tm_year + 1900,
		z->tm_mon + 1, z->tm_mday, z->tm_hour, z->tm_min, z->tm_sec,
		c, (int) (zone / (60*60))
	    );
	    if ((non_hour = zone % (60*60))) {
#		if has_printf_dot
		    static char const fmt[] = ":%.2d";
#		else
		    static char const fmt[] = ":%02d";
#		endif
		VOID sprintf(datebuf + strlen(datebuf), fmt, non_hour / 60);
		if ((non_hour %= 60))
		    VOID sprintf(datebuf + strlen(datebuf), fmt, non_hour);
	    }
	}
	return datebuf;
}
