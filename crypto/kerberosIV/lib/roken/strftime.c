/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#define TM_YEAR_BASE	1900	/* from <tzfile.h> */
#include <string.h>

static char *afmt[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};
static char *Afmt[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
	"Saturday",
};
static char *bfmt[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec",
};
static char *Bfmt[] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December",
};

static size_t gsize;
static char *pt;

static int _add (char *);
static int _conv (int, int, int);
#ifdef	HAVE_MKTIME
static int _secs (const struct tm *);
#endif	/* HAVE_MKTIME */
static size_t _fmt (const char *, const struct tm *);

size_t
strftime(char *s, size_t maxsize, const char *format, const struct tm *t)
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

static size_t
_fmt(const char *format, const struct tm *t)
{
	for (; *format; ++format) {
		if (*format == '%')
			switch(*++format) {
			case '\0':
				--format;
				break;
			case 'A':
				if (t->tm_wday < 0 || t->tm_wday > 6)
					return(0);
				if (!_add(Afmt[t->tm_wday]))
					return(0);
				continue;
			case 'a':
				if (t->tm_wday < 0 || t->tm_wday > 6)
					return(0);
				if (!_add(afmt[t->tm_wday]))
					return(0);
				continue;
			case 'B':
				if (t->tm_mon < 0 || t->tm_mon > 11)
					return(0);
				if (!_add(Bfmt[t->tm_mon]))
					return(0);
				continue;
			case 'b':
			case 'h':
				if (t->tm_mon < 0 || t->tm_mon > 11)
					return(0);
				if (!_add(bfmt[t->tm_mon]))
					return(0);
				continue;
			case 'C':
				if (!_fmt("%a %b %e %H:%M:%S %Y", t))
					return(0);
				continue;
			case 'c':
				if (!_fmt("%m/%d/%y %H:%M:%S", t))
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
				    t->tm_hour % 12 : 12, 2, ' '))
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
				if (!_add(t->tm_hour >= 12 ? "PM" : "AM"))
					return(0);
				continue;
			case 'R':
				if (!_fmt("%H:%M", t))
					return(0);
				continue;
			case 'r':
				if (!_fmt("%I:%M:%S %p", t))
					return(0);
				continue;
			case 'S':
				if (!_conv(t->tm_sec, 2, '0'))
					return(0);
				continue;
#ifdef HAVE_MKTIME
			case 's':
				if (!_secs(t))
					return(0);
				continue;
#endif	/* HAVE_MKTIME */
			case 'T':
			case 'X':
				if (!_fmt("%H:%M:%S", t))
					return(0);
				continue;
			case 't':
				if (!_add("\t"))
					return(0);
				continue;
			case 'U':
				if (!_conv((t->tm_yday + 7 - t->tm_wday) / 7,
				    2, '0'))
					return(0);
				continue;
			case 'W':
				if (!_conv((t->tm_yday + 7 -
				    (t->tm_wday ? (t->tm_wday - 1) : 6))
				    / 7, 2, '0'))
					return(0);
				continue;
			case 'w':
				if (!_conv(t->tm_wday, 1, '0'))
					return(0);
				continue;
			case 'x':
				if (!_fmt("%m/%d/%y", t))
					return(0);
				continue;
			case 'y':
				if (!_conv((t->tm_year + TM_YEAR_BASE)
				    % 100, 2, '0'))
					return(0);
				continue;
			case 'Y':
				if (!_conv(t->tm_year + TM_YEAR_BASE, 4, '0'))
					return(0);
				continue;
#ifdef notdef
			case 'Z':
				if (!t->tm_zone || !_add(t->tm_zone))
					return(0);
				continue;
#endif
			case '%':
			/*
			 * X311J/88-090 (4.12.3.5): if conversion char is
			 * undefined, behavior is undefined.  Print out the
			 * character itself as printf(3) does.
			 */
			default:
				break;
		}
		if (!gsize--)
			return(0);
		*pt++ = *format;
	}
	return(gsize);
}

#ifdef HAVE_MKTIME
static int
_secs(const struct tm *t)
{
	static char buf[15];
	time_t s;
	char *p;
	struct tm tmp;

	/* Make a copy, mktime(3) modifies the tm struct. */
	tmp = *t;
	s = mktime(&tmp);
	for (p = buf + sizeof(buf) - 2; s > 0 && p > buf; s /= 10)
		*p-- = s % 10 + '0';
	return(_add(++p));
}
#endif	/* HAVE_MKTIME */

static int
_conv(int n, int digits, int pad)
{
	static char buf[10];
	char *p;

	for (p = buf + sizeof(buf) - 2; n > 0 && p > buf; n /= 10, --digits)
		*p-- = n % 10 + '0';
	while (p > buf && digits-- > 0)
		*p-- = pad;
	return(_add(++p));
}

static int
_add(str)
	char *str;
{
	for (;; ++pt, --gsize) {
		if (!gsize)
			return(0);
		if (!(*pt = *str++))
			return(1);
	}
}
