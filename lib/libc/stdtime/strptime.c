/*
 * Powerdog Industries kindly requests feedback from anyone modifying
 * this function:
 *
 * Date: Thu, 05 Jun 1997 23:17:17 -0400  
 * From: Kevin Ruddy <kevin.ruddy@powerdog.com>
 * To: James FitzGibbon <james@nexis.net>
 * Subject: Re: Use of your strptime(3) code (fwd)
 * 
 * The reason for the "no mod" clause was so that modifications would
 * come back and we could integrate them and reissue so that a wider 
 * audience could use it (thereby spreading the wealth).  This has   
 * made it possible to get strptime to work on many operating systems.
 * I'm not sure why that's "plain unacceptable" to the FreeBSD team.
 * 
 * Anyway, you can change it to "with or without modification" as
 * you see fit.  Enjoy.                                          
 * 
 * Kevin Ruddy
 * Powerdog Industries, Inc.
 */
/*
 * Copyright (c) 1994 Powerdog Industries.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      This product includes software developed by Powerdog Industries.
 * 4. The name of Powerdog Industries may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY POWERDOG INDUSTRIES ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE POWERDOG INDUSTRIES BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef LIBC_RCS
static const char rcsid[] =
  "$FreeBSD$";
#endif

#ifndef lint
#ifndef NOID
static char copyright[] =
"@(#) Copyright (c) 1994 Powerdog Industries.  All rights reserved.";
static char sccsid[] = "@(#)strptime.c	0.1 (Powerdog) 94/03/27";
#endif /* !defined NOID */
#endif /* not lint */

#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef	_THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"
#endif
#include "timelocal.h"

static char * _strptime(const char *, const char *, struct tm *);

#ifdef	_THREAD_SAFE
static struct pthread_mutex	_gotgmt_mutexd = PTHREAD_MUTEX_STATIC_INITIALIZER;
static pthread_mutex_t		gotgmt_mutex   = &_gotgmt_mutexd;
#endif
static int got_GMT;

#define asizeof(a)	(sizeof (a) / sizeof ((a)[0]))

static char *
_strptime(const char *buf, const char *fmt, struct tm *tm)
{
	char	c;
	const char *ptr;
	int	i,
		len;
	int Ealternative, Oalternative;

	ptr = fmt;
	while (*ptr != 0) {
		if (*buf == 0)
			break;

		c = *ptr++;

		if (c != '%') {
			if (isspace((unsigned char)c))
				while (*buf != 0 && isspace((unsigned char)*buf))
					buf++;
			else if (c != *buf++)
				return 0;
			continue;
		}

		Ealternative = 0;
		Oalternative = 0;
label:
		c = *ptr++;
		switch (c) {
		case 0:
		case '%':
			if (*buf++ != '%')
				return 0;
			break;

		case '+':
			buf = _strptime(buf, Locale->date_fmt, tm);
			if (buf == 0)
				return 0;
			break;

		case 'C':
			if (!isdigit((unsigned char)*buf))
				return 0;

			/* XXX This will break for 3-digit centuries. */
			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 19)
				return 0;

			tm->tm_year = i * 100 - 1900;
			break;

		case 'c':
			/* NOTE: c_fmt is intentionally ignored */
			buf = _strptime(buf, "%a %Ef %T %Y", tm);
			if (buf == 0)
				return 0;
			break;

		case 'D':
			buf = _strptime(buf, "%m/%d/%y", tm);
			if (buf == 0)
				return 0;
			break;

		case 'E':
			if (Ealternative || Oalternative)
				break;
			Ealternative++;
			goto label;

		case 'O':
			if (Ealternative || Oalternative)
				break;
			Oalternative++;
			goto label;

		case 'F':
		case 'f':
			if (!Ealternative)
				break;
			buf = _strptime(buf, (c == 'f') ? Locale->Ef_fmt : Locale->EF_fmt, tm);
			if (buf == 0)
				return 0;
			break;

		case 'R':
			buf = _strptime(buf, "%H:%M", tm);
			if (buf == 0)
				return 0;
			break;

		case 'r':
			buf = _strptime(buf, "%I:%M:%S %p", tm);
			if (buf == 0)
				return 0;
			break;

		case 'T':
			buf = _strptime(buf, "%H:%M:%S", tm);
			if (buf == 0)
				return 0;
			break;

		case 'X':
			buf = _strptime(buf, Locale->X_fmt, tm);
			if (buf == 0)
				return 0;
			break;

		case 'x':
			buf = _strptime(buf, Locale->x_fmt, tm);
			if (buf == 0)
				return 0;
			break;

		case 'j':
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 3;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 1 || i > 366)
				return 0;

			tm->tm_yday = i - 1;
			break;

		case 'M':
		case 'S':
			if (*buf == 0 || isspace((unsigned char)*buf))
				break;

			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}

			if (c == 'M') {
				if (i > 59)
					return 0;
				tm->tm_min = i;
			} else {
				if (i > 60)
					return 0;
				tm->tm_sec = i;
			}

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'H':
		case 'I':
		case 'k':
		case 'l':
			/*
			 * Of these, %l is the only specifier explicitly
			 * documented as not being zero-padded.  However,
			 * there is no harm in allowing zero-padding.
			 *
			 * XXX The %l specifier may gobble one too many
			 * digits if used incorrectly.
			 */
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (c == 'H' || c == 'k') {
				if (i > 23)
					return 0;
			} else if (i > 12)
				return 0;

			tm->tm_hour = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'p':
			/*
			 * XXX This is bogus if parsed before hour-related
			 * specifiers.
			 */
			len = strlen(Locale->am);
			if (strncasecmp(buf, Locale->am, len) == 0) {
				if (tm->tm_hour > 12)
					return 0;
				if (tm->tm_hour == 12)
					tm->tm_hour = 0;
				buf += len;
				break;
			}

			len = strlen(Locale->pm);
			if (strncasecmp(buf, Locale->pm, len) == 0) {
				if (tm->tm_hour > 12)
					return 0;
				if (tm->tm_hour != 12)
					tm->tm_hour += 12;
				buf += len;
				break;
			}

			return 0;

		case 'A':
		case 'a':
			for (i = 0; i < asizeof(Locale->weekday); i++) {
				if (c == 'A') {
					len = strlen(Locale->weekday[i]);
					if (strncasecmp(buf,
							Locale->weekday[i],
							len) == 0)
						break;
				} else {
					len = strlen(Locale->wday[i]);
					if (strncasecmp(buf,
							Locale->wday[i],
							len) == 0)
						break;
				}
			}
			if (i == asizeof(Locale->weekday))
				return 0;

			tm->tm_wday = i;
			buf += len;
			break;

		case 'U':
		case 'W':
			/*
			 * XXX This is bogus, as we can not assume any valid
			 * information present in the tm structure at this
			 * point to calculate a real value, so just check the
			 * range for now.
			 */
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i > 53)
				return 0;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'w':
			if (!isdigit((unsigned char)*buf))
				return 0;

			i = *buf - '0';
			if (i > 6)
				return 0;

			tm->tm_wday = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'd':
		case 'e':
			/*
			 * The %e specifier is explicitly documented as not
			 * being zero-padded but there is no harm in allowing
			 * such padding.
			 *
			 * XXX The %e specifier may gobble one too many
			 * digits if used incorrectly.
			 */
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i > 31)
				return 0;

			tm->tm_mday = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'B':
		case 'b':
		case 'h':
			for (i = 0; i < asizeof(Locale->month); i++) {
				if (Oalternative) {
					if (c == 'B') {
						len = strlen(Locale->alt_month[i]);
						if (strncasecmp(buf,
								Locale->alt_month[i],
								len) == 0)
							break;
					}
				} else {
					if (c == 'B') {
						len = strlen(Locale->month[i]);
						if (strncasecmp(buf,
								Locale->month[i],
								len) == 0)
							break;
					} else {
						len = strlen(Locale->mon[i]);
						if (strncasecmp(buf,
								Locale->mon[i],
								len) == 0)
							break;
					}
				}
			}
			if (i == asizeof(Locale->month))
				return 0;

			tm->tm_mon = i;
			buf += len;
			break;

		case 'm':
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 1 || i > 12)
				return 0;

			tm->tm_mon = i - 1;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 's':
			{
			char *cp;
			time_t t;

			t = strtol(buf, &cp, 10);
			if (t == LONG_MAX)
				return 0;
			buf = cp;
			gmtime_r(&t, tm);
			got_GMT = 1;
			}
			break;

		case 'Y':
		case 'y':
			if (*buf == 0 || isspace((unsigned char)*buf))
				break;

			if (!isdigit((unsigned char)*buf))
				return 0;

			len = (c == 'Y') ? 4 : 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (c == 'Y')
				i -= 1900;
			if (c == 'y' && i < 69)
				i += 100;
			if (i < 0)
				return 0;

			tm->tm_year = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'Z':
			{
			const char *cp;
			char *zonestr;

			for (cp = buf; *cp && isupper((unsigned char)*cp); ++cp) {/*empty*/}
			if (cp - buf) {
				zonestr = alloca(cp - buf + 1);
				strncpy(zonestr, buf, cp - buf);
				zonestr[cp - buf] = '\0';
				tzset();
				if (0 == strcmp(zonestr, "GMT")) {
				    got_GMT = 1;
				} else if (0 == strcmp(zonestr, tzname[0])) {
				    tm->tm_isdst = 0;
				} else if (0 == strcmp(zonestr, tzname[1])) {
				    tm->tm_isdst = 1;
				} else {
				    return 0;
				}
				buf += cp - buf;
			}
			}
			break;
		}
	}
	return (char *)buf;
}


char *
strptime(const char *buf, const char *fmt, struct tm *tm)
{
	char *ret;

#ifdef	_THREAD_SAFE
	pthread_mutex_lock(&gotgmt_mutex);
#endif

	got_GMT = 0;
	ret = _strptime(buf, fmt, tm);
	if (ret && got_GMT) {
		time_t t = timegm(tm);
	    localtime_r(&t, tm);
		got_GMT = 0;
	}

#ifdef	_THREAD_SAFE
	pthread_mutex_unlock(&gotgmt_mutex);
#endif

	return ret;
}
