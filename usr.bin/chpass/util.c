/*-
 * Copyright (c) 1988, 1993, 1994
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)util.c	8.4 (Berkeley) 4/2/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "chpass.h"
#include "pathnames.h"

static char *months[] =
	{ "January", "February", "March", "April", "May", "June",
	  "July", "August", "September", "October", "November",
	  "December", NULL };

char *
ttoa(tval)
	time_t tval;
{
	struct tm *tp;
	static char tbuf[50];

	if (tval) {
		tp = localtime(&tval);
		(void)sprintf(tbuf, "%s %d, %d", months[tp->tm_mon],
		    tp->tm_mday, tp->tm_year + 1900);
	}
	else
		*tbuf = '\0';
	return (tbuf);
}

int
atot(p, store)
	char *p;
	time_t *store;
{
	static struct tm *lt;
	char *t, **mp;
	time_t tval;
	int day, month, year;

	if (!*p) {
		*store = 0;
		return (0);
	}
	if (!lt) {
		unsetenv("TZ");
		(void)time(&tval);
		lt = localtime(&tval);
	}
	if (!(t = strtok(p, " \t")))
		goto bad;
	if (isdigit(*t)) {
		month = atoi(t);
	} else {
		for (mp = months;; ++mp) {
			if (!*mp)
				goto bad;
			if (!strncasecmp(*mp, t, 3)) {
				month = mp - months + 1;
				break;
			}
		}
	}
	if (!(t = strtok((char *)NULL, " \t,")) || !isdigit(*t))
		goto bad;
	day = atoi(t);
	if (!(t = strtok((char *)NULL, " \t,")) || !isdigit(*t))
		goto bad;
	year = atoi(t);
	if (day < 1 || day > 31 || month < 1 || month > 12 || !year)
		goto bad;
	/* Allow two digit years 1969-2068 */
	if (year < 69)
		year += 2000;
	else if (year < 100)
		year += 1900;
	if (year < 1969)
bad:		return (1);
	lt->tm_year = year - 1900;
	lt->tm_mon = month - 1;
	lt->tm_mday = day;
	lt->tm_hour = 0;
	lt->tm_min = 0;
	lt->tm_sec = 0;
	lt->tm_isdst = -1;
	if ((tval = mktime(lt)) < 0)
		return (1);
	*store = tval;
	return (0);
}

char *
ok_shell(name)
	char *name;
{
	char *p, *sh;

	setusershell();
	while ((sh = getusershell())) {
		if (!strcmp(name, sh))
			return (name);
		/* allow just shell name, but use "real" path */
		if ((p = strrchr(sh, '/')) && strcmp(name, p + 1) == 0)
			return (sh);
	}
	return (NULL);
}
