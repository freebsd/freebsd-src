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
static char sccsid[] = "@(#)strftime.c	5.11 (Berkeley) 2/24/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <tzfile.h>
#include <string.h>

static char *abday[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};
static char *day[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
	"Saturday",
};
static char *abmon[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec",
};
static char *mon[] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December",
};
static char *ampm[] = {
	"AM", "PM",
};

static const char *d_t_fmt = "%a %b %e %H:%M:%S %Y";
static const char *d_fmt = "%m/%d/%y";
static const char *t_fmt = "%H:%M:%S";
static const char *t_fmt_ampm = "%I:%M:%S %p";

static size_t gsize;
static char *pt;
static int _add(), _conv(), _secs();
static size_t _fmt();

size_t
strftime(s, maxsize, format, t)
	char *s;
	size_t maxsize;
	const char *format;
	const struct tm *t;
{
	pt = s;
	if ((gsize = maxsize) < 1)
		return(0);
	if (_fmt(format, t)) {
		*pt = '\0';
		return(maxsize - gsize);
	}
	return(0);
}

#define SUN_WEEK(t)	(((t)->tm_yday + 7 - \
				((t)->tm_wday)) / 7)
#define MON_WEEK(t)	(((t)->tm_yday + 7 - \
				((t)->tm_wday ? (t)->tm_wday - 1 : 6)) / 7)
static size_t
_fmt(format, t)
	register char *format;
	struct tm *t;
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
			switch(*format) {
			case '\0':
				--format;
				break;
			case 'A':
				if (t->tm_wday < 0 || t->tm_wday > 6)
					return(0);
				if (!_add(day[t->tm_wday]))
					return(0);
				continue;
			case 'a':
				if (t->tm_wday < 0 || t->tm_wday > 6)
					return(0);
				if (!_add(abday[t->tm_wday]))
					return(0);
				continue;
			case 'B':
				if (t->tm_mon < 0 || t->tm_mon > 11)
					return(0);
				if (!_add(mon[t->tm_mon]))
					return(0);
				continue;
			case 'b':
			case 'h':
				if (t->tm_mon < 0 || t->tm_mon > 11)
					return(0);
				if (!_add(abmon[t->tm_mon]))
					return(0);
				continue;
			case 'C':
				if (!_conv((t->tm_year + TM_YEAR_BASE) / 100,
				    2, '0'))
					return(0);
				continue;
			case 'c':
				if (!_fmt(d_t_fmt, t))
					return(0);
				continue;
			case 'D':
				if (!_fmt("%m/%d/%y", t))
					return(0);
				continue;
			case 'd':
				if (!_conv(t->tm_mday, 2, '0'))
					return(0);
				continue;
			case 'e':
				if (!_conv(t->tm_mday, 2, ' '))
					return(0);
				continue;
			case 'H':
				if (!_conv(t->tm_hour, 2, '0'))
					return(0);
				continue;
			case 'I':
				if (!_conv(t->tm_hour % 12 ?
				    t->tm_hour % 12 : 12, 2, '0'))
					return(0);
				continue;
			case 'j':
				if (!_conv(t->tm_yday + 1, 3, '0'))
					return(0);
				continue;
			case 'k':
				if (!_conv(t->tm_hour, 2, ' '))
					return(0);
				continue;
			case 'l':
				if (!_conv(t->tm_hour % 12 ?
				    t->tm_hour % 12: 12, 2, ' '))
					return(0);
				continue;
			case 'M':
				if (!_conv(t->tm_min, 2, '0'))
					return(0);
				continue;
			case 'm':
				if (!_conv(t->tm_mon + 1, 2, '0'))
					return(0);
				continue;
			case 'n':
				if (!_add("\n"))
					return(0);
				continue;
			case 'p':
				if (!_add(ampm[t->tm_hour >= 12]))
					return(0);
				continue;
			case 'R':
				if (!_fmt("%H:%M", t))
					return(0);
				continue;
			case 'r':
				if (!_fmt(t_fmt_ampm, t))
					return(0);
				continue;
			case 'S':
				if (!_conv(t->tm_sec, 2, '0'))
					return(0);
				continue;
			case 's':
				if (!_secs(t))
					return(0);
				continue;
			case 'T':
				if (!_fmt("%H:%M:%S", t))
					return(0);
				continue;
			case 't':
				if (!_add("\t"))
					return(0);
				continue;
			case 'U':
				if (!_conv(SUN_WEEK(t), 2, '0'))
					return(0);
				continue;
			case 'u':
				if (!_conv(t->tm_wday ? t->tm_wday : 7, 2, '0'))
					return(0);
				continue;
			case 'V':
				{
				/* ISO 8601 Week Of Year:
				   If the week (Monday - Sunday) containing
				   January 1 has four or more days in the new 
				   year, then it is week 1; otherwise it is 
				   week 53 of the previous year and the next
				   week is week one. */
				 
				int week = MON_WEEK(t);

				if (((t->tm_yday + 7 - (t->tm_wday + 1)) % 7) >= 4) {
					week++;
				} else if (week == 0) {
					week = 53;
				}

				if (!_conv(week, 2, '0'))
					return(0);
				continue;
				}
			case 'W':
				if (!_conv(MON_WEEK(t), 2, '0'))
					return(0);
				continue;
			case 'w':
				if (!_conv(t->tm_wday, 1, '0'))
					return(0);
				continue;
			case 'x':
				if (!_fmt(d_fmt, t))
					return(0);
				continue;
			case 'X':
				if (!_fmt(t_fmt, t))
					return(0);
				continue;
			case 'y':
				if (!_conv((t->tm_year + TM_YEAR_BASE) % 100,
				    2, '0'))
					return(0);
				continue;
			case 'Y':
				if (!_conv((t->tm_year + TM_YEAR_BASE), 4, '0'))
					return(0);
				continue;
			case 'Z':
				if (t->tm_zone && !_add(t->tm_zone))
					return(0);
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
		if (!gsize--)
			return(0);
		*pt++ = *format;
	}
	return(gsize);
}

static
_secs(t)
	struct tm *t;
{
	static char buf[15];
	register time_t s;
	register char *p;
	struct tm tmp;

	/* Make a copy, mktime(3) modifies the tm struct. */
	tmp = *t;
	s = mktime(&tmp);
	for (p = buf + sizeof(buf) - 2; s > 0 && p > buf; s /= 10)
		*p-- = s % 10 + '0';
	return(_add(++p));
}

static
_conv(n, digits, pad)
	int n, digits;
	char pad;
{
	static char buf[10];
	register char *p;

	for (p = buf + sizeof(buf) - 2; n > 0 && p > buf; n /= 10, --digits)
		*p-- = n % 10 + '0';
	while (p > buf && digits-- > 0)
		*p-- = pad;
	return(_add(++p));
}

static
_add(str)
	register char *str;
{
	for (;; ++pt, --gsize) {
		if (!gsize)
			return(0);
		if (!(*pt = *str++))
			return(1);
	}
}
