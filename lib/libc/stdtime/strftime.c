#ifndef lint
#ifndef NOID
static char	elsieid[] = "@(#)strftime.c	7.19";
/*
** Based on the UCB version with the ID appearing below.
** This is ANSIish only when time is treated identically in all locales and
** when "multibyte character == plain character".
*/
#endif /* !defined NOID */
#endif /* !defined lint */

#include "private.h"

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef LIBC_SCCS
#ifndef lint
static const char sccsid[] = "@(#)strftime.c	5.4 (Berkeley) 3/14/89";
#endif /* !defined lint */
#endif /* !defined LIBC_SCCS */

#include "tzfile.h"

static const char afmt[][4] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char Afmt[][10] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
	"Saturday"
};
static const char bfmt[][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec"
};
static const char Bfmt[][10] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December"
};

static char *_add P((const char *, char *, const char *));
static char *_conv P((int, const char *, char *, const char *));
static char *_fmt P((const char *, const struct tm *, char *, const char *));
static char *_secs P((const struct tm *, char *, const char *));

size_t strftime P((char *, size_t, const char *, const struct tm *));

extern char *tzname[];

size_t
strftime(s, maxsize, format, t)
	char *s;
	size_t maxsize;
	const char *format;
	const struct tm *t;
{
	char *p;

	p = _fmt(format, t, s, s + maxsize);
	if (p == s + maxsize)
		return 0;
	*p = '\0';
	return p - s;
}

static char *
_fmt(format, t, pt, ptlim)
	const char *format;
	const struct tm *t;
	char *pt;
	const char *ptlim;
{
	for (; *format; ++format) {
		if (*format == '%') {
label:
			switch(*++format) {
			case '\0':
				--format;
				break;
			case 'A':
				pt = _add((t->tm_wday < 0 || t->tm_wday > 6) ?
					"?" : Afmt[t->tm_wday], pt, ptlim);
				continue;
			case 'a':
				pt = _add((t->tm_wday < 0 || t->tm_wday > 6) ?
					"?" : afmt[t->tm_wday], pt, ptlim);
				continue;
			case 'B':
				pt = _add((t->tm_mon < 0 || t->tm_mon > 11) ?
					"?" : Bfmt[t->tm_mon], pt, ptlim);
				continue;
			case 'b':
			case 'h':
				pt = _add((t->tm_mon < 0 || t->tm_mon > 11) ?
					"?" : bfmt[t->tm_mon], pt, ptlim);
				continue;
			case 'c':
				pt = _fmt("%D %X", t, pt, ptlim);
				continue;
			case 'C':
				/*
				** %C used to do a...
				**	_fmt("%a %b %e %X %Y", t);
				** ...whereas now POSIX 1003.2 calls for
				** something completely different.
				** (ado, 5/24/93)
				*/
				pt = _conv((t->tm_year + TM_YEAR_BASE) / 100,
					"%02d", pt, ptlim);
				continue;
			case 'D':
				pt = _fmt("%m/%d/%y", t, pt, ptlim);
				continue;
			case 'x':
				/*
				** Version 3.0 of strftime from Arnold Robbins
				** (arnold@skeeve.atl.ga.us) does the
				** equivalent of...
				**	_fmt("%a %b %e %Y");
				** ...for %x; since the X3J11 C language
				** standard calls for "date, using locale's
				** date format," anything goes.  Using just
				** numbers (as here) makes Quakers happier.
				** Word from Paul Eggert (eggert@twinsun.com)
				** is that %Y-%m-%d is the ISO standard date
				** format, specified in ISO 2014 and later
				** ISO 8601:1988, with a summary available in
				** pub/doc/ISO/english/ISO8601.ps.Z on
				** ftp.uni-erlangen.de.
				** (ado, 5/30/93)
				*/
				pt = _fmt("%m/%d/%y", t, pt, ptlim);
				continue;
			case 'd':
				pt = _conv(t->tm_mday, "%02d", pt, ptlim);
				continue;
			case 'E':
			case 'O':
				/*
				** POSIX locale extensions, a la
				** Arnold Robbins' strftime version 3.0.
				** The sequences
				**	%Ec %EC %Ex %Ey %EY
				**	%Od %oe %OH %OI %Om %OM
				**	%OS %Ou %OU %OV %Ow %OW %Oy
				** are supposed to provide alternate
				** representations.
				** (ado, 5/24/93)
				*/
				goto label;
			case 'e':
				pt = _conv(t->tm_mday, "%2d", pt, ptlim);
				continue;
			case 'H':
				pt = _conv(t->tm_hour, "%02d", pt, ptlim);
				continue;
			case 'I':
				pt = _conv((t->tm_hour % 12) ?
					(t->tm_hour % 12) : 12,
					"%02d", pt, ptlim);
				continue;
			case 'j':
				pt = _conv(t->tm_yday + 1, "%03d", pt, ptlim);
				continue;
			case 'k':
				/*
				** This used to be...
				**	_conv(t->tm_hour % 12 ?
				**		t->tm_hour % 12 : 12, 2, ' ');
				** ...and has been changed to the below to
				** match SunOS 4.1.1 and Arnold Robbins'
				** strftime version 3.0.  That is, "%k" and
				** "%l" have been swapped.
				** (ado, 5/24/93)
				*/
				pt = _conv(t->tm_hour, "%2d", pt, ptlim);
				continue;
#ifdef KITCHEN_SINK
			case 'K':
				/*
				** After all this time, still unclaimed!
				*/
				pt = _add("kitchen sink", pt, ptlim);
				continue;
#endif /* defined KITCHEN_SINK */
			case 'l':
				/*
				** This used to be...
				**	_conv(t->tm_hour, 2, ' ');
				** ...and has been changed to the below to
				** match SunOS 4.1.1 and Arnold Robbin's
				** strftime version 3.0.  That is, "%k" and
				** "%l" have been swapped.
				** (ado, 5/24/93)
				*/
				pt = _conv((t->tm_hour % 12) ?
					(t->tm_hour % 12) : 12,
					"%2d", pt, ptlim);
				continue;
			case 'M':
				pt = _conv(t->tm_min, "%02d", pt, ptlim);
				continue;
			case 'm':
				pt = _conv(t->tm_mon + 1, "%02d", pt, ptlim);
				continue;
			case 'n':
				pt = _add("\n", pt, ptlim);
				continue;
			case 'p':
				pt = _add(t->tm_hour >= 12 ? "PM" : "AM",
					pt, ptlim);
				continue;
			case 'R':
				pt = _fmt("%H:%M", t, pt, ptlim);
				continue;
			case 'r':
				pt = _fmt("%I:%M:%S %p", t, pt, ptlim);
				continue;
			case 'S':
				pt = _conv(t->tm_sec, "%02d", pt, ptlim);
				continue;
			case 's':
				pt = _secs(t, pt, ptlim);
				continue;
			case 'T':
			case 'X':
				pt = _fmt("%H:%M:%S", t, pt, ptlim);
				continue;
			case 't':
				pt = _add("\t", pt, ptlim);
				continue;
			case 'U':
				pt = _conv((t->tm_yday + 7 - t->tm_wday) / 7,
					"%02d", pt, ptlim);
				continue;
			case 'u':
				/*
				** From Arnold Robbins' strftime version 3.0:
				** "ISO 8601: Weekday as a decimal number
				** [1 (Monday) - 7]"
				** (ado, 5/24/93)
				*/
				pt = _conv((t->tm_wday == 0) ? 7 : t->tm_wday,
					"%d", pt, ptlim);
				continue;
			case 'V':
				/*
				** From Arnold Robbins' strftime version 3.0:
				** "the week number of the year (the first
				** Monday as the first day of week 1) as a
				** decimal number (01-53).  The method for
				** determining the week number is as specified
				** by ISO 8601 (to wit: if the week containing
				** January 1 has four or more days in the new
				** year, then it is week 1, otherwise it is
				** week 53 of the previous year and the next
				** week is week 1)."
				** (ado, 5/24/93)
				*/
				/*
				** XXX--If January 1 falls on a Friday,
				** January 1-3 are part of week 53 of the
				** previous year.  By analogy, if January
				** 1 falls on a Thursday, are December 29-31
				** of the PREVIOUS year part of week 1???
				** (ado 5/24/93)
				**
				** You are understood not to expect this.
				*/
				{
					int i;

					i = (t->tm_yday + 10 - (t->tm_wday ?
						(t->tm_wday - 1) : 6)) / 7;
					if (i == 0) {
						/*
						** What day of the week does
						** January 1 fall on?
						*/
						i = t->tm_wday -
							(t->tm_yday - 1);
						/*
						** Fri Jan 1: 53
						** Sun Jan 1: 52
						** Sat Jan 1: 53 if previous
						** 		 year a leap
						**		 year, else 52
						*/
						if (i == TM_FRIDAY)
							i = 53;
						else if (i == TM_SUNDAY)
							i = 52;
						else	i = isleap(t->tm_year +
								TM_YEAR_BASE) ?
								53 : 52;
#ifdef XPG4_1994_04_09
						/*
						** As of 4/9/94, though,
						** XPG4 calls for 53
						** unconditionally.
						*/
						i = 53;
#endif /* defined XPG4_1994_04_09 */
					}
					pt = _conv(i, "%02d", pt, ptlim);
				}
				continue;
			case 'v':
				/*
				** From Arnold Robbins' strftime version 3.0:
				** "date as dd-bbb-YYYY"
				** (ado, 5/24/93)
				*/
				pt = _fmt("%e-%b-%Y", t, pt, ptlim);
				continue;
			case 'W':
				pt = _conv((t->tm_yday + 7 -
					(t->tm_wday ?
					(t->tm_wday - 1) : 6)) / 7,
					"%02d", pt, ptlim);
				continue;
			case 'w':
				pt = _conv(t->tm_wday, "%d", pt, ptlim);
				continue;
			case 'y':
				pt = _conv((t->tm_year + TM_YEAR_BASE) % 100,
					"%02d", pt, ptlim);
				continue;
			case 'Y':
				pt = _conv(t->tm_year + TM_YEAR_BASE, "%04d",
					pt, ptlim);
				continue;
			case 'Z':
#ifdef TM_ZONE
				if (t->TM_ZONE)
					pt = _add(t->TM_ZONE, pt, ptlim);
				else
#endif /* defined TM_ZONE */
				if (t->tm_isdst == 0 || t->tm_isdst == 1) {
					pt = _add(tzname[t->tm_isdst],
						pt, ptlim);
				} else  pt = _add("?", pt, ptlim);
				continue;
			case '%':
			/*
			 * X311J/88-090 (4.12.3.5): if conversion char is
			 * undefined, behavior is undefined.  Print out the
			 * character itself as printf(3) also does.
			 */
			default:
				break;
			}
		}
		if (pt == ptlim)
			break;
		*pt++ = *format;
	}
	return pt;
}

static char *
_conv(n, format, pt, ptlim)
	int n;
	const char *format;
	char *pt;
	const char *ptlim;
{
	char buf[INT_STRLEN_MAXIMUM(int) + 1];

	(void) sprintf(buf, format, n);
	return _add(buf, pt, ptlim);
}

static char *
_secs(t, pt, ptlim)
	const struct tm *t;
	char *pt;
	const char *ptlim;
{
	static char buf[INT_STRLEN_MAXIMUM(int) + 1];
	register time_t s;
	register char *p;
	struct tm tmp;

	/* Make a copy, mktime(3) modifies the tm struct. */
	tmp = *t;
	s = mktime(&tmp);
	(void) sprintf(buf, "%d", s);
	return(_add(buf, pt, ptlim));
}

static char *
_add(str, pt, ptlim)
	const char *str;
	char *pt;
	const char *ptlim;
{
	while (pt < ptlim && (*pt = *str++) != '\0')
		++pt;
	return pt;
}
