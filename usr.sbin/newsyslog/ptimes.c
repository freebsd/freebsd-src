/*-
 * ------+---------+---------+---------+---------+---------+---------+---------*
 * Initial version of parse8601 was originally added to newsyslog.c in
 *     FreeBSD on Jan 22, 1999 by Garrett Wollman <wollman@FreeBSD.org>.
 * Initial version of parseDWM was originally added to newsyslog.c in
 *     FreeBSD on Apr  4, 2000 by Hellmuth Michaelis <hm@FreeBSD.org>.
 *
 * Copyright (c) 2003  - Garance Alistair Drosehn <gad@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the FreeBSD Project.
 *
 * ------+---------+---------+---------+---------+---------+---------+---------*
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "extern.h"

/*-
 * Parse a limited subset of ISO 8601. The specific format is as follows:
 *
 * [CC[YY[MM[DD]]]][THH[MM[SS]]]	(where `T' is the literal letter)
 *
 * We don't accept a timezone specification; missing fields (including timezone)
 * are defaulted to the current date but time zero.
 */
time_t
parse8601(const char *s, time_t *next_time)
{
	char *t;
	time_t tsecs;
	struct tm tm, *tmp;
	long l;

	tmp = localtime(&timenow);
	tm = *tmp;
	if (next_time != NULL)
		*next_time = (time_t)-1;

	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	l = strtol(s, &t, 10);
	if (l < 0 || l >= INT_MAX || (*t != '\0' && *t != 'T'))
		return (-1);

	/*
	 * Now t points either to the end of the string (if no time was
	 * provided) or to the letter `T' which separates date and time in
	 * ISO 8601.  The pointer arithmetic is the same for either case.
	 */
	switch (t - s) {
	case 8:
		tm.tm_year = ((l / 1000000) - 19) * 100;
		l = l % 1000000;
	case 6:
		tm.tm_year -= tm.tm_year % 100;
		tm.tm_year += l / 10000;
		l = l % 10000;
	case 4:
		tm.tm_mon = (l / 100) - 1;
		l = l % 100;
	case 2:
		tm.tm_mday = l;
	case 0:
		break;
	default:
		return (-1);
	}

	/* sanity check */
	if (tm.tm_year < 70 || tm.tm_mon < 0 || tm.tm_mon > 12
	    || tm.tm_mday < 1 || tm.tm_mday > 31)
		return (-1);

	if (*t != '\0') {
		s = ++t;
		l = strtol(s, &t, 10);
		if (l < 0 || l >= INT_MAX || (*t != '\0' && !isspace(*t)))
			return (-1);

		switch (t - s) {
		case 6:
			tm.tm_sec = l % 100;
			l /= 100;
		case 4:
			tm.tm_min = l % 100;
			l /= 100;
		case 2:
			tm.tm_hour = l;
		case 0:
			break;
		default:
			return (-1);
		}

		/* sanity check */
		if (tm.tm_sec < 0 || tm.tm_sec > 60 || tm.tm_min < 0
		    || tm.tm_min > 59 || tm.tm_hour < 0 || tm.tm_hour > 23)
			return (-1);
	}

	tsecs = mktime(&tm);
	/*
	 * Check for invalid times, including things like the missing
	 * hour when switching from "standard time" to "daylight saving".
	 */
	if (tsecs == (time_t)-1)
		tsecs = (time_t)-2;
	return (tsecs);
}

/*-
 * Parse a cyclic time specification, the format is as follows:
 *
 *	[Dhh] or [Wd[Dhh]] or [Mdd[Dhh]]
 *
 * to rotate a logfile cyclic at
 *
 *	- every day (D) within a specific hour (hh)	(hh = 0...23)
 *	- once a week (W) at a specific day (d)     OR	(d = 0..6, 0 = Sunday)
 *	- once a month (M) at a specific day (d)	(d = 1..31,l|L)
 *
 * We don't accept a timezone specification; missing fields
 * are defaulted to the current date but time zero.
 */
time_t
parseDWM(char *s, time_t *next_time)
{
	char *t;
	time_t tsecs;
	struct tm tm, *tmp;
	long l;
	int nd;
	static int mtab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int WMseen = 0;
	int Dseen = 0;

	tmp = localtime(&timenow);
	tm = *tmp;
	if (next_time != NULL)
		*next_time = (time_t)-1;

	/* set no. of days per month */

	nd = mtab[tm.tm_mon];

	if (tm.tm_mon == 1) {
		if (((tm.tm_year + 1900) % 4 == 0) &&
		    ((tm.tm_year + 1900) % 100 != 0) &&
		    ((tm.tm_year + 1900) % 400 == 0)) {
			nd++;	/* leap year, 29 days in february */
		}
	}
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	for (;;) {
		switch (*s) {
		case 'D':
			if (Dseen)
				return (-1);
			Dseen++;
			s++;
			l = strtol(s, &t, 10);
			if (l < 0 || l > 23)
				return (-1);
			tm.tm_hour = l;
			break;

		case 'W':
			if (WMseen)
				return (-1);
			WMseen++;
			s++;
			l = strtol(s, &t, 10);
			if (l < 0 || l > 6)
				return (-1);
			if (l != tm.tm_wday) {
				int save;

				if (l < tm.tm_wday) {
					save = 6 - tm.tm_wday;
					save += (l + 1);
				} else {
					save = l - tm.tm_wday;
				}

				tm.tm_mday += save;

				if (tm.tm_mday > nd) {
					tm.tm_mon++;
					tm.tm_mday = tm.tm_mday - nd;
				}
			}
			break;

		case 'M':
			if (WMseen)
				return (-1);
			WMseen++;
			s++;
			if (tolower(*s) == 'l') {
				tm.tm_mday = nd;
				s++;
				t = s;
			} else {
				l = strtol(s, &t, 10);
				if (l < 1 || l > 31)
					return (-1);

				if (l > nd)
					return (-1);
				tm.tm_mday = l;
			}
			break;

		default:
			return (-1);
			break;
		}

		if (*t == '\0' || isspace(*t))
			break;
		else
			s = t;
	}

	tsecs = mktime(&tm);
	/*
	 * Check for invalid times, including things like the missing
	 * hour when switching from "standard time" to "daylight saving".
	 */
	if (tsecs == (time_t)-1)
		tsecs = (time_t)-2;
	return (tsecs);
}
