/* Convert a broken-down timestamp to a string.  */

/* Copyright 1989 The Regents of the University of California.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the University nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.  */

/*
** Based on the UCB version with the copyright notice appearing above.
**
** This is ANSIish only when "multibyte character == plain character".
*/

#include "private.h"

#include <fcntl.h>
#include <locale.h>
#include <stdio.h>

#ifndef DEPRECATE_TWO_DIGIT_YEARS
# define DEPRECATE_TWO_DIGIT_YEARS false
#endif

struct lc_time_T {
	const char *	mon[MONSPERYEAR];
	const char *	month[MONSPERYEAR];
	const char *	wday[DAYSPERWEEK];
	const char *	weekday[DAYSPERWEEK];
	const char *	X_fmt;
	const char *	x_fmt;
	const char *	c_fmt;
	const char *	am;
	const char *	pm;
	const char *	date_fmt;
};

static const struct lc_time_T	C_time_locale = {
	{
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	}, {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	}, {
		"Sun", "Mon", "Tue", "Wed",
		"Thu", "Fri", "Sat"
	}, {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"
	},

	/* X_fmt */
	"%H:%M:%S",

	/*
	** x_fmt
	** C99 and later require this format.
	** Using just numbers (as here) makes Quakers happier;
	** it's also compatible with SVR4.
	*/
	"%m/%d/%y",

	/*
	** c_fmt
	** C99 and later require this format.
	** Previously this code used "%D %X", but we now conform to C99.
	** Note that
	**	"%a %b %d %H:%M:%S %Y"
	** is used by Solaris 2.3.
	*/
	"%a %b %e %T %Y",

	/* am */
	"AM",

	/* pm */
	"PM",

	/* date_fmt */
	"%a %b %e %H:%M:%S %Z %Y"
};

enum warn { IN_NONE, IN_SOME, IN_THIS, IN_ALL };

static char *	_add(const char *, char *, const char *);
static char *	_conv(int, const char *, char *, const char *);
static char *	_fmt(const char *, const struct tm *, char *, const char *,
		     enum warn *);
static char *	_yconv(int, int, bool, bool, char *, char const *);

#ifndef YEAR_2000_NAME
# define YEAR_2000_NAME "CHECK_STRFTIME_FORMATS_FOR_TWO_DIGIT_YEARS"
#endif /* !defined YEAR_2000_NAME */

#if HAVE_STRFTIME_L
size_t
strftime_l(char *restrict s, size_t maxsize, char const *restrict format,
	   struct tm const *restrict t,
	   ATTRIBUTE_MAYBE_UNUSED locale_t locale)
{
  /* Just call strftime, as only the C locale is supported.  */
  return strftime(s, maxsize, format, t);
}
#endif

size_t
strftime(char *restrict s, size_t maxsize, char const *restrict format,
	 struct tm const *restrict t)
{
	char *	p;
	int saved_errno = errno;
	enum warn warn = IN_NONE;

	tzset();
	p = _fmt(format, t, s, s + maxsize, &warn);
	if (!p) {
	  errno = EOVERFLOW;
	  return 0;
	}
	if (DEPRECATE_TWO_DIGIT_YEARS
	    && warn != IN_NONE && getenv(YEAR_2000_NAME)) {
		fprintf(stderr, "\n");
		fprintf(stderr, "strftime format \"%s\" ", format);
		fprintf(stderr, "yields only two digits of years in ");
		if (warn == IN_SOME)
			fprintf(stderr, "some locales");
		else if (warn == IN_THIS)
			fprintf(stderr, "the current locale");
		else	fprintf(stderr, "all locales");
		fprintf(stderr, "\n");
	}
	if (p == s + maxsize) {
		errno = ERANGE;
		return 0;
	}
	*p = '\0';
	errno = saved_errno;
	return p - s;
}

static char *
_fmt(const char *format, const struct tm *t, char *pt,
     const char *ptlim, enum warn *warnp)
{
	struct lc_time_T const *Locale = &C_time_locale;

	for ( ; *format; ++format) {
		if (*format == '%') {
label:
			switch (*++format) {
			case '\0':
				--format;
				break;
			case 'A':
				pt = _add((t->tm_wday < 0 ||
					t->tm_wday >= DAYSPERWEEK) ?
					"?" : Locale->weekday[t->tm_wday],
					pt, ptlim);
				continue;
			case 'a':
				pt = _add((t->tm_wday < 0 ||
					t->tm_wday >= DAYSPERWEEK) ?
					"?" : Locale->wday[t->tm_wday],
					pt, ptlim);
				continue;
			case 'B':
				pt = _add((t->tm_mon < 0 ||
					t->tm_mon >= MONSPERYEAR) ?
					"?" : Locale->month[t->tm_mon],
					pt, ptlim);
				continue;
			case 'b':
			case 'h':
				pt = _add((t->tm_mon < 0 ||
					t->tm_mon >= MONSPERYEAR) ?
					"?" : Locale->mon[t->tm_mon],
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
				pt = _yconv(t->tm_year, TM_YEAR_BASE,
					    true, false, pt, ptlim);
				continue;
			case 'c':
				{
				enum warn warn2 = IN_SOME;

				pt = _fmt(Locale->c_fmt, t, pt, ptlim, &warn2);
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
				pt = _conv(t->tm_mday, "%02d", pt, ptlim);
				continue;
			case 'E':
			case 'O':
				/*
				** Locale modifiers of C99 and later.
				** The sequences
				**	%Ec %EC %Ex %EX %Ey %EY
				**	%Od %oe %OH %OI %Om %OM
				**	%OS %Ou %OU %OV %Ow %OW %Oy
				** are supposed to provide alternative
				** representations.
				*/
				goto label;
			case 'e':
				pt = _conv(t->tm_mday, "%2d", pt, ptlim);
				continue;
			case 'F':
				pt = _fmt("%Y-%m-%d", t, pt, ptlim, warnp);
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
				** strftime version 3.0. That is, "%k" and
				** "%l" have been swapped.
				** (ado, 1993-05-24)
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
				** strftime version 3.0. That is, "%k" and
				** "%l" have been swapped.
				** (ado, 1993-05-24)
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
				pt = _add((t->tm_hour >= (HOURSPERDAY / 2)) ?
					Locale->pm :
					Locale->am,
					pt, ptlim);
				continue;
			case 'R':
				pt = _fmt("%H:%M", t, pt, ptlim, warnp);
				continue;
			case 'r':
				pt = _fmt("%I:%M:%S %p", t, pt, ptlim, warnp);
				continue;
			case 'S':
				pt = _conv(t->tm_sec, "%02d", pt, ptlim);
				continue;
			case 's':
				{
					struct tm	tm;
					char		buf[INT_STRLEN_MAXIMUM(
								time_t) + 1];
					time_t		mkt;

					tm.tm_sec = t->tm_sec;
					tm.tm_min = t->tm_min;
					tm.tm_hour = t->tm_hour;
					tm.tm_mday = t->tm_mday;
					tm.tm_mon = t->tm_mon;
					tm.tm_year = t->tm_year;
#ifdef TM_GMTOFF
					mkt = timeoff(&tm, t->TM_GMTOFF);
#else
					tm.tm_isdst = t->tm_isdst;
					mkt = mktime(&tm);
#endif
					/* If mktime fails, %s expands to the
					   value of (time_t) -1 as a failure
					   marker; this is better in practice
					   than strftime failing.  */
					if (TYPE_SIGNED(time_t)) {
					  intmax_t n = mkt;
					  sprintf(buf, "%"PRIdMAX, n);
					} else {
					  uintmax_t n = mkt;
					  sprintf(buf, "%"PRIuMAX, n);
					}
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
					"%02d", pt, ptlim);
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
** From Arnold Robbins' strftime version 3.0: "the week number of the
** year (the first Monday as the first day of week 1) as a decimal number
** (01-53)."
** (ado, 1993-05-24)
**
** From <https://www.cl.cam.ac.uk/~mgk25/iso-time.html> by Markus Kuhn:
** "Week 01 of a year is per definition the first week which has the
** Thursday in this year, which is equivalent to the week which contains
** the fourth day of January. In other words, the first week of a new year
** is the week which has the majority of its days in the new year. Week 01
** might also contain days from the previous year and the week before week
** 01 of a year is the last week (52 or 53) of the previous year even if
** it contains days from the new year. A week starts with Monday (day 1)
** and ends with Sunday (day 7). For example, the first week of the year
** 1997 lasts from 1996-12-30 to 1997-01-05..."
** (ado, 1996-01-02)
*/
				{
					int	year;
					int	base;
					int	yday;
					int	wday;
					int	w;

					year = t->tm_year;
					base = TM_YEAR_BASE;
					yday = t->tm_yday;
					wday = t->tm_wday;
					for ( ; ; ) {
						int	len;
						int	bot;
						int	top;

						len = isleap_sum(year, base) ?
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
							++base;
							w = 1;
							break;
						}
						if (yday >= bot) {
							w = 1 + ((yday - bot) /
								DAYSPERWEEK);
							break;
						}
						--base;
						yday += isleap_sum(year, base) ?
							DAYSPERLYEAR :
							DAYSPERNYEAR;
					}
#ifdef XPG4_1994_04_09
					if ((w == 52 &&
						t->tm_mon == TM_JANUARY) ||
						(w == 1 &&
						t->tm_mon == TM_DECEMBER))
							w = 53;
#endif /* defined XPG4_1994_04_09 */
					if (*format == 'V')
						pt = _conv(w, "%02d",
							pt, ptlim);
					else if (*format == 'g') {
						*warnp = IN_ALL;
						pt = _yconv(year, base,
							false, true,
							pt, ptlim);
					} else	pt = _yconv(year, base,
							true, true,
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
					"%02d", pt, ptlim);
				continue;
			case 'w':
				pt = _conv(t->tm_wday, "%d", pt, ptlim);
				continue;
			case 'X':
				pt = _fmt(Locale->X_fmt, t, pt, ptlim, warnp);
				continue;
			case 'x':
				{
				enum warn warn2 = IN_SOME;

				pt = _fmt(Locale->x_fmt, t, pt, ptlim, &warn2);
				if (warn2 == IN_ALL)
					warn2 = IN_THIS;
				if (warn2 > *warnp)
					*warnp = warn2;
				}
				continue;
			case 'y':
				*warnp = IN_ALL;
				pt = _yconv(t->tm_year, TM_YEAR_BASE,
					false, true,
					pt, ptlim);
				continue;
			case 'Y':
				pt = _yconv(t->tm_year, TM_YEAR_BASE,
					true, true,
					pt, ptlim);
				continue;
			case 'Z':
#ifdef TM_ZONE
				pt = _add(t->TM_ZONE, pt, ptlim);
#elif HAVE_TZNAME
				if (t->tm_isdst >= 0)
					pt = _add(tzname[t->tm_isdst != 0],
						pt, ptlim);
#endif
				/*
				** C99 and later say that %Z must be
				** replaced by the empty string if the
				** time zone abbreviation is not
				** determinable.
				*/
				continue;
			case 'z':
#if defined TM_GMTOFF || USG_COMPAT || ALTZONE
				{
				long		diff;
				char const *	sign;
				bool negative;

# ifdef TM_GMTOFF
				diff = t->TM_GMTOFF;
# else
				/*
				** C99 and later say that the UT offset must
				** be computed by looking only at
				** tm_isdst. This requirement is
				** incorrect, since it means the code
				** must rely on magic (in this case
				** altzone and timezone), and the
				** magic might not have the correct
				** offset. Doing things correctly is
				** tricky and requires disobeying the standard;
				** see GNU C strftime for details.
				** For now, punt and conform to the
				** standard, even though it's incorrect.
				**
				** C99 and later say that %z must be replaced by
				** the empty string if the time zone is not
				** determinable, so output nothing if the
				** appropriate variables are not available.
				*/
				if (t->tm_isdst < 0)
					continue;
				if (t->tm_isdst == 0)
#  if USG_COMPAT
					diff = -timezone;
#  else
					continue;
#  endif
				else
#  if ALTZONE
					diff = -altzone;
#  else
					continue;
#  endif
# endif
				negative = diff < 0;
				if (diff == 0) {
# ifdef TM_ZONE
				  negative = t->TM_ZONE[0] == '-';
# else
				  negative = t->tm_isdst < 0;
#  if HAVE_TZNAME
				  if (tzname[t->tm_isdst != 0][0] == '-')
				    negative = true;
#  endif
# endif
				}
				if (negative) {
					sign = "-";
					diff = -diff;
				} else	sign = "+";
				pt = _add(sign, pt, ptlim);
				diff /= SECSPERMIN;
				diff = (diff / MINSPERHOUR) * 100 +
					(diff % MINSPERHOUR);
				pt = _conv(diff, "%04d", pt, ptlim);
				}
#endif
				continue;
			case '+':
				pt = _fmt(Locale->date_fmt, t, pt, ptlim,
					warnp);
				continue;
			case '%':
			/*
			** X311J/88-090 (4.12.3.5): if conversion char is
			** undefined, behavior is undefined. Print out the
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
_conv(int n, const char *format, char *pt, const char *ptlim)
{
	char	buf[INT_STRLEN_MAXIMUM(int) + 1];

	sprintf(buf, format, n);
	return _add(buf, pt, ptlim);
}

static char *
_add(const char *str, char *pt, const char *ptlim)
{
	while (pt < ptlim && (*pt = *str++) != '\0')
		++pt;
	return pt;
}

/*
** POSIX and the C Standard are unclear or inconsistent about
** what %C and %y do if the year is negative or exceeds 9999.
** Use the convention that %C concatenated with %y yields the
** same output as %Y, and that %Y contains at least 4 bytes,
** with more only if necessary.
*/

static char *
_yconv(int a, int b, bool convert_top, bool convert_yy,
       char *pt, const char *ptlim)
{
	register int	lead;
	register int	trail;

	int DIVISOR = 100;
	trail = a % DIVISOR + b % DIVISOR;
	lead = a / DIVISOR + b / DIVISOR + trail / DIVISOR;
	trail %= DIVISOR;
	if (trail < 0 && lead > 0) {
		trail += DIVISOR;
		--lead;
	} else if (lead < 0 && trail > 0) {
		trail -= DIVISOR;
		++lead;
	}
	if (convert_top) {
		if (lead == 0 && trail < 0)
			pt = _add("-0", pt, ptlim);
		else	pt = _conv(lead, "%02d", pt, ptlim);
	}
	if (convert_yy)
		pt = _conv(((trail < 0) ? -trail : trail), "%02d", pt, ptlim);
	return pt;
}
