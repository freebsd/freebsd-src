/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "psdate.h"


static int
a2i(char const ** str)
{
	int             i = 0;
	char const     *s = *str;

	if (isdigit(*s)) {
		i = atoi(s);
		while (isdigit(*s))
			++s;
		*str = s;
	}
	return i;
}

static int
numerics(char const * str)
{
	int             rc = isdigit(*str);

	if (rc)
		while (isdigit(*str) || *str == 'x')
			++str;
	return rc && !*str;
}

static int
aindex(char const * arr[], char const ** str, int len)
{
	int             l, i;
	char            mystr[32];

	mystr[len] = '\0';
	l = strlen(strncpy(mystr, *str, len));
	for (i = 0; i < l; i++)
		mystr[i] = (char) tolower(mystr[i]);
	for (i = 0; arr[i] && strcmp(mystr, arr[i]) != 0; i++);
	if (arr[i] == NULL)
		i = -1;
	else {			/* Skip past it */
		while (**str && isalpha(**str))
			++(*str);
		/* And any following whitespace */
		while (**str && (**str == ',' || isspace(**str)))
			++(*str);
	}			/* Return index */
	return i;
}

static int
weekday(char const ** str)
{
	static char const *days[] =
	{"sun", "mon", "tue", "wed", "thu", "fri", "sat", NULL};

	return aindex(days, str, 3);
}

static int
month(char const ** str)
{
	static char const *months[] =
	{"jan", "feb", "mar", "apr", "may", "jun", "jul",
	"aug", "sep", "oct", "nov", "dec", NULL};

	return aindex(months, str, 3);
}

static void
parse_time(char const * str, int *hour, int *min, int *sec)
{
	*hour = a2i(&str);
	if ((str = strchr(str, ':')) == NULL)
		*min = *sec = 0;
	else {
		++str;
		*min = a2i(&str);
		*sec = ((str = strchr(str, ':')) == NULL) ? 0 : atoi(++str);
	}
}


static void
parse_datesub(char const * str, int *day, int *mon, int *year)
{
	int             i;

	static char const nchrs[] = "0123456789 \t,/-.";

	if ((i = month(&str)) != -1) {
		*mon = i;
		if ((i = a2i(&str)) != 0)
			*day = i;
	} else if ((i = a2i(&str)) != 0) {
		*day = i;
		while (*str && strchr(nchrs + 10, *str) != NULL)
			++str;
		if ((i = month(&str)) != -1)
			*mon = i;
		else if ((i = a2i(&str)) != 0)
			*mon = i - 1;
	} else
		return;

	while (*str && strchr(nchrs + 10, *str) != NULL)
		++str;
	if (isdigit(*str)) {
		*year = atoi(str);
		if (*year > 1900)
			*year -= 1900;
		else if (*year < 32)
			*year += 100;
	}
}


/*-
 * Parse time must be flexible, it handles the following formats:
 * nnnnnnnnnnn		UNIX timestamp (all numeric), 0 = now
 * 0xnnnnnnnn		UNIX timestamp in hexadecimal
 * 0nnnnnnnnn		UNIX timestamp in octal
 * 0			Given time
 * +nnnn[smhdwoy]	Given time + nnnn hours, mins, days, weeks, months or years
 * -nnnn[smhdwoy]	Given time - nnnn hours, mins, days, weeks, months or years
 * dd[ ./-]mmm[ ./-]yy	Date }
 * hh:mm:ss		Time } May be combined
 */

time_t
parse_date(time_t dt, char const * str)
{
	char           *p;
	int             i;
	long            val;
	struct tm      *T;

	if (dt == 0)
		dt = time(NULL);

	while (*str && isspace(*str))
		++str;

	if (numerics(str)) {
		val = strtol(str, &p, 0);
		dt = val ? val : dt;
	} else if (*str == '+' || *str == '-') {
		val = strtol(str, &p, 0);
		switch (*p) {
		case 'h':
		case 'H':	/* hours */
			dt += (val * 3600L);
			break;
		case '\0':
		case 'm':
		case 'M':	/* minutes */
			dt += (val * 60L);
			break;
		case 's':
		case 'S':	/* seconds */
			dt += val;
			break;
		case 'd':
		case 'D':	/* days */
			dt += (val * 86400L);
			break;
		case 'w':
		case 'W':	/* weeks */
			dt += (val * 604800L);
			break;
		case 'o':
		case 'O':	/* months */
			T = localtime(&dt);
			T->tm_mon += (int) val;
			i = T->tm_mday;
			goto fixday;
		case 'y':
		case 'Y':	/* years */
			T = localtime(&dt);
			T->tm_year += (int) val;
			i = T->tm_mday;
	fixday:
			dt = mktime(T);
			T = localtime(&dt);
			if (T->tm_mday != i) {
				T->tm_mday = 1;
				dt = mktime(T);
				dt -= (time_t) 86400L;
			}
		default:	/* unknown */
			break;	/* leave untouched */
		}
	} else {
		char           *q, tmp[64];

		/*
		 * Skip past any weekday prefix
		 */
		weekday(&str);
		str = strncpy(tmp, str, sizeof tmp - 1);
		tmp[sizeof tmp - 1] = '\0';
		T = localtime(&dt);

		/*
		 * See if we can break off any timezone
		 */
		while ((q = strrchr(tmp, ' ')) != NULL) {
			if (strchr("(+-", q[1]) != NULL)
				*q = '\0';
			else {
				int             j = 1;

				while (q[j] && isupper(q[j]))
					++j;
				if (q[j] == '\0')
					*q = '\0';
				else
					break;
			}
		}

		/*
		 * See if there is a time hh:mm[:ss]
		 */
		if ((p = strchr(tmp, ':')) == NULL) {

			/*
			 * No time string involved
			 */
			T->tm_hour = T->tm_min = T->tm_sec = 0;
			parse_datesub(tmp, &T->tm_mday, &T->tm_mon, &T->tm_year);
		} else {
			char            datestr[64], timestr[64];

			/*
			 * Let's chip off the time string
			 */
			if ((q = strpbrk(p, " \t")) != NULL) {	/* Time first? */
				int             l = q - str;

				strncpy(timestr, str, l);
				timestr[l] = '\0';
				strncpy(datestr, q + 1, sizeof datestr);
				datestr[sizeof datestr - 1] = '\0';
				parse_time(timestr, &T->tm_hour, &T->tm_min, &T->tm_sec);
				parse_datesub(datestr, &T->tm_mday, &T->tm_mon, &T->tm_year);
			} else if ((q = strrchr(tmp, ' ')) != NULL) {	/* Time last */
				int             l = q - tmp;

				strncpy(timestr, q + 1, sizeof timestr);
				timestr[sizeof timestr - 1] = '\0';
				strncpy(datestr, tmp, l);
				datestr[l] = '\0';
			} else	/* Bail out */
				return dt;
			parse_time(timestr, &T->tm_hour, &T->tm_min, &T->tm_sec);
			parse_datesub(datestr, &T->tm_mday, &T->tm_mon, &T->tm_year);
		}
		dt = mktime(T);
	}
	return dt;
}
