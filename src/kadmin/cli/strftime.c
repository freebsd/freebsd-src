/* -*- mode: c; c-file-style: "bsd"; indent-tabs-mode: t -*- */
/*	$NetBSD: strftime.c,v 1.8 1999/02/07 17:33:30 augustss Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char *sccsid = "@(#)strftime.c	5.11 (Berkeley) 2/24/91";
#else
__RCSID("$NetBSD: strftime.c,v 1.8 1999/02/07 17:33:30 augustss Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <string.h>
#include <time.h>

/* begin krb5 hack - replace stuff that would come from netbsd libc */
#undef _CurrentTimeLocale
#define _CurrentTimeLocale (&dummy_locale_info)

struct dummy_locale_info_t {
    char d_t_fmt[15];
    char t_fmt_ampm[12];
    char t_fmt[9];
    char d_fmt[9];
    char day[7][10];
    char abday[7][4];
    char mon[12][10];
    char abmon[12][4];
    char am_pm[2][3];
};
static const struct dummy_locale_info_t dummy_locale_info = {
    "%a %b %d %X %Y",		/* %c */
    "%I:%M:%S %p",		/* %r */
    "%H:%M:%S",			/* %X */
    "%m/%d/%y",			/* %x */
    { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
      "Saturday" },
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
    { "January", "February", "March", "April", "May", "June",
      "July", "August", "September", "October", "November", "December" },
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
    { "AM", "PM" },
};
#undef  TM_YEAR_BASE
#define TM_YEAR_BASE 1900

#undef  DAYSPERLYEAR
#define DAYSPERLYEAR 366
#undef  DAYSPERNYEAR
#define DAYSPERNYEAR 365
#undef  DAYSPERWEEK
#define DAYSPERWEEK 7
#undef  isleap
#define isleap(N)	((N % 4) == 0 && (N % 100 != 0 || N % 400 == 0))
#undef  tzname
#define tzname my_tzname
static const char *const tzname[2] = { 0, 0 };
#undef  tzset
#define tzset()
#undef __P
#define __P(X) X /* we already require ansi c in this tree */
/* end krb5 hack */

static	int _add __P((const char *, char **, const char *));
static	int _conv __P((int, int, int, char **, const char *));
static	int _secs __P((const struct tm *, char **, const char *));
static	size_t _fmt __P((const char *, const struct tm *, char **,
	    const char *));

size_t
strftime(s, maxsize, format, t)
	char *s;
	size_t maxsize;
	const char *format;
	const struct tm *t;
{
	char *pt;

	tzset();
	if (maxsize < 1)
		return (0);

	pt = s;
	if (_fmt(format, t, &pt, s + maxsize)) {
		*pt = '\0';
		return (pt - s);
	} else
		return (0);
}

#define SUN_WEEK(t)	(((t)->tm_yday + 7 - \
				((t)->tm_wday)) / 7)
#define MON_WEEK(t)	(((t)->tm_yday + 7 - \
				((t)->tm_wday ? (t)->tm_wday - 1 : 6)) / 7)

static size_t
_fmt(format, t, pt, ptlim)
	const char *format;
	const struct tm *t;
	char **pt;
	const char * const ptlim;
{
	for (; *format; ++format) {
		if (*format == '%') {
			++format;
			if (*format == 'E') {
				/* Alternate Era */
				++format;
			} else if (*format == 'O') {
				/* Alternate numeric symbols */
				++format;
			}
			switch (*format) {
			case '\0':
				--format;
				break;
			case 'A':
				if (t->tm_wday < 0 || t->tm_wday > 6)
					return (0);
				if (!_add(_CurrentTimeLocale->day[t->tm_wday],
				    pt, ptlim))
					return (0);
				continue;

			case 'a':
				if (t->tm_wday < 0 || t->tm_wday > 6)
					return (0);
				if (!_add(_CurrentTimeLocale->abday[t->tm_wday],
				    pt, ptlim))
					return (0);
				continue;
			case 'B':
				if (t->tm_mon < 0 || t->tm_mon > 11)
					return (0);
				if (!_add(_CurrentTimeLocale->mon[t->tm_mon],
				    pt, ptlim))
					return (0);
				continue;
			case 'b':
			case 'h':
				if (t->tm_mon < 0 || t->tm_mon > 11)
					return (0);
				if (!_add(_CurrentTimeLocale->abmon[t->tm_mon],
				    pt, ptlim))
					return (0);
				continue;
			case 'C':
				if (!_conv((t->tm_year + TM_YEAR_BASE) / 100,
				    2, '0', pt, ptlim))
					return (0);
				continue;
			case 'c':
				if (!_fmt(_CurrentTimeLocale->d_t_fmt, t, pt,
				    ptlim))
					return (0);
				continue;
			case 'D':
				if (!_fmt("%m/%d/%y", t, pt, ptlim))
					return (0);
				continue;
			case 'd':
				if (!_conv(t->tm_mday, 2, '0', pt, ptlim))
					return (0);
				continue;
			case 'e':
				if (!_conv(t->tm_mday, 2, ' ', pt, ptlim))
					return (0);
				continue;
			case 'H':
				if (!_conv(t->tm_hour, 2, '0', pt, ptlim))
					return (0);
				continue;
			case 'I':
				if (!_conv(t->tm_hour % 12 ?
				    t->tm_hour % 12 : 12, 2, '0', pt, ptlim))
					return (0);
				continue;
			case 'j':
				if (!_conv(t->tm_yday + 1, 3, '0', pt, ptlim))
					return (0);
				continue;
			case 'k':
				if (!_conv(t->tm_hour, 2, ' ', pt, ptlim))
					return (0);
				continue;
			case 'l':
				if (!_conv(t->tm_hour % 12 ?
				    t->tm_hour % 12: 12, 2, ' ', pt, ptlim))
					return (0);
				continue;
			case 'M':
				if (!_conv(t->tm_min, 2, '0', pt, ptlim))
					return (0);
				continue;
			case 'm':
				if (!_conv(t->tm_mon + 1, 2, '0', pt, ptlim))
					return (0);
				continue;
			case 'n':
				if (!_add("\n", pt, ptlim))
					return (0);
				continue;
			case 'p':
				if (!_add(_CurrentTimeLocale->am_pm[t->tm_hour
				    >= 12], pt, ptlim))
					return (0);
				continue;
			case 'R':
				if (!_fmt("%H:%M", t, pt, ptlim))
					return (0);
				continue;
			case 'r':
				if (!_fmt(_CurrentTimeLocale->t_fmt_ampm, t, pt,
				    ptlim))
					return (0);
				continue;
			case 'S':
				if (!_conv(t->tm_sec, 2, '0', pt, ptlim))
					return (0);
				continue;
			case 's':
				if (!_secs(t, pt, ptlim))
					return (0);
				continue;
			case 'T':
				if (!_fmt("%H:%M:%S", t, pt, ptlim))
					return (0);
				continue;
			case 't':
				if (!_add("\t", pt, ptlim))
					return (0);
				continue;
			case 'U':
				if (!_conv(SUN_WEEK(t), 2, '0', pt, ptlim))
					return (0);
				continue;
			case 'u':
				if (!_conv(t->tm_wday ? t->tm_wday : 7, 1, '0',
				    pt, ptlim))
					return (0);
				continue;
			case 'V':	/* ISO 8601 week number */
			case 'G':	/* ISO 8601 year (four digits) */
			case 'g':	/* ISO 8601 year (two digits) */
/*
** From Arnold Robbins' strftime version 3.0:  "the week number of the
** year (the first Monday as the first day of week 1) as a decimal number
** (01-53)."
** (ado, 1993-05-24)
**
** From "http://www.ft.uni-erlangen.de/~mskuhn/iso-time.html" by Markus Kuhn:
** "Week 01 of a year is per definition the first week which has the
** Thursday in this year, which is equivalent to the week which contains
** the fourth day of January. In other words, the first week of a new year
** is the week which has the majority of its days in the new year. Week 01
** might also contain days from the previous year and the week before week
** 01 of a year is the last week (52 or 53) of the previous year even if
** it contains days from the new year. A week starts with Monday (day 1)
** and ends with Sunday (day 7).  For example, the first week of the year
** 1997 lasts from 1996-12-30 to 1997-01-05..."
** (ado, 1996-01-02)
*/
				{
					int	year;
					int	yday;
					int	wday;
					int	w;

					year = t->tm_year + TM_YEAR_BASE;
					yday = t->tm_yday;
					wday = t->tm_wday;
					for ( ; ; ) {
						int	len;
						int	bot;
						int	top;

						len = isleap(year) ?
							DAYSPERLYEAR :
							DAYSPERNYEAR;
						/*
						** What yday (-3 ... 3) does
						** the ISO year begin on?
						*/
						bot = ((yday + 11 - wday) %
							DAYSPERWEEK) - 3;
						/*
						** What yday does the NEXT
						** ISO year begin on?
						*/
						top = bot -
							(len % DAYSPERWEEK);
						if (top < -3)
							top += DAYSPERWEEK;
						top += len;
						if (yday >= top) {
							++year;
							w = 1;
							break;
						}
						if (yday >= bot) {
							w = 1 + ((yday - bot) /
								DAYSPERWEEK);
							break;
						}
						--year;
						yday += isleap(year) ?
							DAYSPERLYEAR :
							DAYSPERNYEAR;
					}
#ifdef XPG4_1994_04_09
					if ((w == 52
					     && t->tm_mon == TM_JANUARY)
					    || (w == 1
						&& t->tm_mon == TM_DECEMBER))
						w = 53;
#endif /* defined XPG4_1994_04_09 */
					if (*format == 'V') {
						if (!_conv(w, 2, '0',
							pt, ptlim))
							return (0);
					} else if (*format == 'g') {
						if (!_conv(year % 100, 2, '0',
							pt, ptlim))
							return (0);
					} else	if (!_conv(year, 4, '0',
							pt, ptlim))
							return (0);
				}
				continue;
			case 'W':
				if (!_conv(MON_WEEK(t), 2, '0', pt, ptlim))
					return (0);
				continue;
			case 'w':
				if (!_conv(t->tm_wday, 1, '0', pt, ptlim))
					return (0);
				continue;
			case 'x':
				if (!_fmt(_CurrentTimeLocale->d_fmt, t, pt,
				    ptlim))
					return (0);
				continue;
			case 'X':
				if (!_fmt(_CurrentTimeLocale->t_fmt, t, pt,
				    ptlim))
					return (0);
				continue;
			case 'y':
				if (!_conv((t->tm_year + TM_YEAR_BASE) % 100,
				    2, '0', pt, ptlim))
					return (0);
				continue;
			case 'Y':
				if (!_conv((t->tm_year + TM_YEAR_BASE), 4, '0',
				    pt, ptlim))
					return (0);
				continue;
			case 'Z':
				if (tzname[t->tm_isdst ? 1 : 0] &&
				    !_add(tzname[t->tm_isdst ? 1 : 0], pt,
				    ptlim))
					return (0);
				continue;
			case '%':
			/*
			 * X311J/88-090 (4.12.3.5): if conversion char is
			 * undefined, behavior is undefined.  Print out the
			 * character itself as printf(3) does.
			 */
			default:
				break;
			}
		}
		if (*pt == ptlim)
			return (0);
		*(*pt)++ = *format;
	}
	return (ptlim - *pt);
}

static int
_secs(t, pt, ptlim)
	const struct tm *t;
	char **pt;
	const char * const ptlim;
{
	char buf[15];
	time_t s;
	char *p;
	struct tm tmp;

	buf[sizeof (buf) - 1] = '\0';
	/* Make a copy, mktime(3) modifies the tm struct. */
	tmp = *t;
	s = mktime(&tmp);
	for (p = buf + sizeof(buf) - 2; s > 0 && p > buf; s /= 10)
		*p-- = (char)(s % 10 + '0');
	return (_add(++p, pt, ptlim));
}

static int
_conv(n, digits, pad, pt, ptlim)
	int n, digits;
	int pad;
	char **pt;
	const char * const ptlim;
{
	char buf[10];
	char *p;

	buf[sizeof (buf) - 1] = '\0';
	for (p = buf + sizeof(buf) - 2; n > 0 && p > buf; n /= 10, --digits)
		*p-- = n % 10 + '0';
	while (p > buf && digits-- > 0)
		*p-- = pad;
	return (_add(++p, pt, ptlim));
}

static int
_add(str, pt, ptlim)
	const char *str;
	char **pt;
	const char * const ptlim;
{

	for (;; ++(*pt)) {
		if (*pt == ptlim)
			return (0);
		if ((**pt = *str++) == '\0')
			return (1);
	}
}
