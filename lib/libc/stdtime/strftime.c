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

#ifndef lint
#ifndef NOID
static const char	elsieid[] = "@(#)strftime.c	7.64";
/*
** Based on the UCB version with the ID appearing below.
** This is ANSIish only when "multibyte character == plain character".
*/
#endif /* !defined NOID */
#endif /* !defined lint */

#include "namespace.h"
#include "private.h"

#if defined(LIBC_SCCS) && !defined(lint)
static const char	sccsid[] = "@(#)strftime.c	5.4 (Berkeley) 3/14/89";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "tzfile.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "un-namespace.h"
#include "timelocal.h"

static char *	_add(const char *, char *, const char *);
static char *	_conv(int, const char *, char *, const char *);
static char *	_fmt(const char *, const struct tm *, char *, const char *, int *);

size_t strftime(char * __restrict, size_t, const char * __restrict,
    const struct tm * __restrict);

extern char *	tzname[];

#ifndef YEAR_2000_NAME
#define YEAR_2000_NAME	"CHECK_STRFTIME_FORMATS_FOR_TWO_DIGIT_YEARS"
#endif /* !defined YEAR_2000_NAME */


#define IN_NONE	0
#define IN_SOME	1
#define IN_THIS	2
#define IN_ALL	3

#define PAD_DEFAULT 0
#define PAD_LESS	1
#define PAD_SPACE	2
#define PAD_ZERO	3

static const char* fmt_padding[][4] = {
	/* DEFAULT,	LESS,	SPACE,	ZERO */
#define PAD_FMT_MONTHDAY	0
#define PAD_FMT_HMS			0
#define PAD_FMT_CENTURY		0
#define PAD_FMT_SHORTYEAR	0
#define PAD_FMT_MONTH		0
#define PAD_FMT_WEEKOFYEAR	0
#define PAD_FMT_DAYOFMONTH	0
	{ "%02d",	"%d",	"%2d",	"%02d" },
#define PAD_FMT_SDAYOFMONTH	1
#define PAD_FMT_SHMS		1
	{ "%2d",	"%d",	"%2d",	"%02d" },
#define	PAD_FMT_DAYOFYEAR	2
	{ "%03d",	"%d",	"%3d",	"%03d" },
#define PAD_FMT_YEAR		3
	{ "%04d",	"%d",	"%4d",	"%04d" }
};

size_t
strftime(char * __restrict s, size_t maxsize, const char * __restrict format,
    const struct tm * __restrict t)
{
	char *	p;
	int	warn;

	tzset();
	warn = IN_NONE;
	p = _fmt(((format == NULL) ? "%c" : format), t, s, s + maxsize, &warn);
#ifndef NO_RUN_TIME_WARNINGS_ABOUT_YEAR_2000_PROBLEMS_THANK_YOU
	if (warn != IN_NONE && getenv(YEAR_2000_NAME) != NULL) {
		(void) fprintf(stderr, "\n");
		if (format == NULL)
			(void) fprintf(stderr, "NULL strftime format ");
		else	(void) fprintf(stderr, "strftime format \"%s\" ",
				format);
		(void) fprintf(stderr, "yields only two digits of years in ");
		if (warn == IN_SOME)
			(void) fprintf(stderr, "some locales");
		else if (warn == IN_THIS)
			(void) fprintf(stderr, "the current locale");
		else	(void) fprintf(stderr, "all locales");
		(void) fprintf(stderr, "\n");
	}
#endif /* !defined NO_RUN_TIME_WARNINGS_ABOUT_YEAR_2000_PROBLEMS_THANK_YOU */
	if (p == s + maxsize)
		return 0;
	*p = '\0';
	return p - s;
}

static char *
_fmt(format, t, pt, ptlim, warnp)
const char *		format;
const struct tm * const	t;
char *			pt;
const char * const	ptlim;
int *			warnp;
{
	int Ealternative, Oalternative, PadIndex;
	struct lc_time_T *tptr = __get_current_time_locale();

	for ( ; *format; ++format) {
		if (*format == '%') {
			Ealternative = 0;
			Oalternative = 0;
			PadIndex	 = PAD_DEFAULT;
label:
			switch (*++format) {
			case '\0':
				--format;
				break;
			case 'A':
				pt = _add((t->tm_wday < 0 ||
					t->tm_wday >= DAYSPERWEEK) ?
					"?" : tptr->weekday[t->tm_wday],
					pt, ptlim);
				continue;
			case 'a':
				pt = _add((t->tm_wday < 0 ||
					t->tm_wday >= DAYSPERWEEK) ?
					"?" : tptr->wday[t->tm_wday],
					pt, ptlim);
				continue;
			case 'B':
				pt = _add((t->tm_mon < 0 ||
					t->tm_mon >= MONSPERYEAR) ?
					"?" : (Oalternative ? tptr->alt_month :
					tptr->month)[t->tm_mon],
					pt, ptlim);
				continue;
			case 'b':
			case 'h':
				pt = _add((t->tm_mon < 0 ||
					t->tm_mon >= MONSPERYEAR) ?
					"?" : tptr->mon[t->tm_mon],
					pt, ptlim);
				continue;
			case 'C':
				/*
				** %C used to do a...
				**	_fmt("%a %b %e %X %Y", t);
				** ...whereas now POSIX 1003.2 calls for
				** something completely different.
				** (ado, 1993-05-24)
				*/
				pt = _conv((t->tm_year + TM_YEAR_BASE) / 100,
					fmt_padding[PAD_FMT_CENTURY][PadIndex], pt, ptlim);
				continue;
			case 'c':
				{
				int warn2 = IN_SOME;

				pt = _fmt(tptr->c_fmt, t, pt, ptlim, warnp);
				if (warn2 == IN_ALL)
					warn2 = IN_THIS;
				if (warn2 > *warnp)
					*warnp = warn2;
				}
				continue;
			case 'D':
				pt = _fmt("%m/%d/%y", t, pt, ptlim, warnp);
				continue;
			case 'd':
				pt = _conv(t->tm_mday, fmt_padding[PAD_FMT_DAYOFMONTH][PadIndex],
					pt, ptlim);
				continue;
			case 'E':
				if (Ealternative || Oalternative)
					break;
				Ealternative++;
				goto label;
			case 'O':
				/*
				** C99 locale modifiers.
				** The sequences
				**	%Ec %EC %Ex %EX %Ey %EY
				**	%Od %oe %OH %OI %Om %OM
				**	%OS %Ou %OU %OV %Ow %OW %Oy
				** are supposed to provide alternate
				** representations.
				**
				** FreeBSD extension
				**      %OB
				*/
				if (Ealternative || Oalternative)
					break;
				Oalternative++;
				goto label;
			case 'e':
				pt = _conv(t->tm_mday,
					fmt_padding[PAD_FMT_SDAYOFMONTH][PadIndex], pt, ptlim);
				continue;
			case 'F':
				pt = _fmt("%Y-%m-%d", t, pt, ptlim, warnp);
				continue;
			case 'H':
				pt = _conv(t->tm_hour, fmt_padding[PAD_FMT_HMS][PadIndex],
					pt, ptlim);
				continue;
			case 'I':
				pt = _conv((t->tm_hour % 12) ?
					(t->tm_hour % 12) : 12,
					fmt_padding[PAD_FMT_HMS][PadIndex], pt, ptlim);
				continue;
			case 'j':
				pt = _conv(t->tm_yday + 1,
					fmt_padding[PAD_FMT_DAYOFYEAR][PadIndex], pt, ptlim);
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
				** (ado, 1993-05-24)
				*/
				pt = _conv(t->tm_hour, fmt_padding[PAD_FMT_SHMS][PadIndex],
					pt, ptlim);
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
				** (ado, 1993-05-24)
				*/
				pt = _conv((t->tm_hour % 12) ?
					(t->tm_hour % 12) : 12,
					fmt_padding[PAD_FMT_SHMS][PadIndex], pt, ptlim);
				continue;
			case 'M':
				pt = _conv(t->tm_min, fmt_padding[PAD_FMT_HMS][PadIndex],
					pt, ptlim);
				continue;
			case 'm':
				pt = _conv(t->tm_mon + 1,
					fmt_padding[PAD_FMT_MONTH][PadIndex], pt, ptlim);
				continue;
			case 'n':
				pt = _add("\n", pt, ptlim);
				continue;
			case 'p':
				pt = _add((t->tm_hour >= (HOURSPERDAY / 2)) ?
					tptr->pm :
					tptr->am,
					pt, ptlim);
				continue;
			case 'R':
				pt = _fmt("%H:%M", t, pt, ptlim, warnp);
				continue;
			case 'r':
				pt = _fmt(tptr->ampm_fmt, t, pt, ptlim,
					warnp);
				continue;
			case 'S':
				pt = _conv(t->tm_sec, fmt_padding[PAD_FMT_HMS][PadIndex],
					pt, ptlim);
				continue;
			case 's':
				{
					struct tm	tm;
					char		buf[INT_STRLEN_MAXIMUM(
								time_t) + 1];
					time_t		mkt;

					tm = *t;
					mkt = mktime(&tm);
					if (TYPE_SIGNED(time_t))
						(void) sprintf(buf, "%ld",
							(long) mkt);
					else	(void) sprintf(buf, "%lu",
							(unsigned long) mkt);
					pt = _add(buf, pt, ptlim);
				}
				continue;
			case 'T':
				pt = _fmt("%H:%M:%S", t, pt, ptlim, warnp);
				continue;
			case 't':
				pt = _add("\t", pt, ptlim);
				continue;
			case 'U':
				pt = _conv((t->tm_yday + DAYSPERWEEK -
					t->tm_wday) / DAYSPERWEEK,
					fmt_padding[PAD_FMT_WEEKOFYEAR][PadIndex], pt, ptlim);
				continue;
			case 'u':
				/*
				** From Arnold Robbins' strftime version 3.0:
				** "ISO 8601: Weekday as a decimal number
				** [1 (Monday) - 7]"
				** (ado, 1993-05-24)
				*/
				pt = _conv((t->tm_wday == 0) ?
					DAYSPERWEEK : t->tm_wday,
					"%d", pt, ptlim);
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
					if (*format == 'V')
						pt = _conv(w, fmt_padding[PAD_FMT_WEEKOFYEAR][PadIndex],
							pt, ptlim);
					else if (*format == 'g') {
						*warnp = IN_ALL;
						pt = _conv(year % 100, fmt_padding[PAD_FMT_SHORTYEAR][PadIndex],
							pt, ptlim);
					} else	pt = _conv(year, fmt_padding[PAD_FMT_YEAR][PadIndex],
							pt, ptlim);
				}
				continue;
			case 'v':
				/*
				** From Arnold Robbins' strftime version 3.0:
				** "date as dd-bbb-YYYY"
				** (ado, 1993-05-24)
				*/
				pt = _fmt("%e-%b-%Y", t, pt, ptlim, warnp);
				continue;
			case 'W':
				pt = _conv((t->tm_yday + DAYSPERWEEK -
					(t->tm_wday ?
					(t->tm_wday - 1) :
					(DAYSPERWEEK - 1))) / DAYSPERWEEK,
					fmt_padding[PAD_FMT_WEEKOFYEAR][PadIndex], pt, ptlim);
				continue;
			case 'w':
				pt = _conv(t->tm_wday, "%d", pt, ptlim);
				continue;
			case 'X':
				pt = _fmt(tptr->X_fmt, t, pt, ptlim, warnp);
				continue;
			case 'x':
				{
				int	warn2 = IN_SOME;

				pt = _fmt(tptr->x_fmt, t, pt, ptlim, &warn2);
				if (warn2 == IN_ALL)
					warn2 = IN_THIS;
				if (warn2 > *warnp)
					*warnp = warn2;
				}
				continue;
			case 'y':
				*warnp = IN_ALL;
				pt = _conv((t->tm_year + TM_YEAR_BASE) % 100,
					fmt_padding[PAD_FMT_SHORTYEAR][PadIndex], pt, ptlim);
				continue;
			case 'Y':
				pt = _conv(t->tm_year + TM_YEAR_BASE,
					fmt_padding[PAD_FMT_YEAR][PadIndex],
					pt, ptlim);
				continue;
			case 'Z':
#ifdef TM_ZONE
				if (t->TM_ZONE != NULL)
					pt = _add(t->TM_ZONE, pt, ptlim);
				else
#endif /* defined TM_ZONE */
				if (t->tm_isdst >= 0)
					pt = _add(tzname[t->tm_isdst != 0],
						pt, ptlim);
				/*
				** C99 says that %Z must be replaced by the
				** empty string if the time zone is not
				** determinable.
				*/
				continue;
			case 'z':
				{
				int		diff;
				char const *	sign;

				if (t->tm_isdst < 0)
					continue;
#ifdef TM_GMTOFF
				diff = t->TM_GMTOFF;
#else /* !defined TM_GMTOFF */
				/*
				** C99 says that the UTC offset must
				** be computed by looking only at
				** tm_isdst.  This requirement is
				** incorrect, since it means the code
				** must rely on magic (in this case
				** altzone and timezone), and the
				** magic might not have the correct
				** offset.  Doing things correctly is
				** tricky and requires disobeying C99;
				** see GNU C strftime for details.
				** For now, punt and conform to the
				** standard, even though it's incorrect.
				**
				** C99 says that %z must be replaced by the
				** empty string if the time zone is not
				** determinable, so output nothing if the
				** appropriate variables are not available.
				*/
				if (t->tm_isdst == 0)
#ifdef USG_COMPAT
					diff = -timezone;
#else /* !defined USG_COMPAT */
					continue;
#endif /* !defined USG_COMPAT */
				else
#ifdef ALTZONE
					diff = -altzone;
#else /* !defined ALTZONE */
					continue;
#endif /* !defined ALTZONE */
#endif /* !defined TM_GMTOFF */
				if (diff < 0) {
					sign = "-";
					diff = -diff;
				} else	sign = "+";
				pt = _add(sign, pt, ptlim);
				diff /= 60;
				pt = _conv((diff/60)*100 + diff%60,
					fmt_padding[PAD_FMT_YEAR][PadIndex], pt, ptlim);
				}
				continue;
			case '+':
				pt = _fmt(tptr->date_fmt, t, pt, ptlim,
					warnp);
				continue;
			case '-':
				if (PadIndex != PAD_DEFAULT)
					break;
				PadIndex = PAD_LESS;
				goto label;
			case '_':
				if (PadIndex != PAD_DEFAULT)
					break;
				PadIndex = PAD_SPACE;
				goto label;
			case '0':
				if (PadIndex != PAD_DEFAULT)
					break;
				PadIndex = PAD_ZERO;
				goto label;
			case '%':
			/*
			** X311J/88-090 (4.12.3.5): if conversion char is
			** undefined, behavior is undefined.  Print out the
			** character itself as printf(3) also does.
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
const int		n;
const char * const	format;
char * const		pt;
const char * const	ptlim;
{
	char	buf[INT_STRLEN_MAXIMUM(int) + 1];

	(void) sprintf(buf, format, n);
	return _add(buf, pt, ptlim);
}

static char *
_add(str, pt, ptlim)
const char *		str;
char *			pt;
const char * const	ptlim;
{
	while (pt < ptlim && (*pt = *str++) != '\0')
		++pt;
	return pt;
}
