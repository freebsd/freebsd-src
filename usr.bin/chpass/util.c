/*-
 * Copyright (c) 1988 The Regents of the University of California.
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

#ifndef lint
static char sccsid[] = "@(#)util.c	5.14 (Berkeley) 2/12/91";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <tzfile.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chpass.h"
#include "pathnames.h"

static int dmsize[] =
	{ -1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
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
		    tp->tm_mday, tp->tm_year + TM_YEAR_BASE);
	}
	else
		*tbuf = '\0';
	return(tbuf);
} 

atot(p, store)
	char *p;
	time_t *store;
{
	register char *t, **mp;
	static struct tm *lt;
	time_t tval, time();
	int day, month, year;

	if (!*p) {
		*store = 0;
		return(0);
	}
	if (!lt) {
		unsetenv("TZ");
		(void)time(&tval);
		lt = localtime(&tval);
	}
	if (!(t = strtok(p, " \t")))
		goto bad;
	for (mp = months;; ++mp) {
		if (!*mp)
			goto bad;
		if (!strncasecmp(*mp, t, 3)) {
			month = mp - months + 1;
			break;
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
	if (year < 100)
		year += TM_YEAR_BASE;
	if (year <= EPOCH_YEAR)
bad:		return(1);
	tval = isleap(year) && month > 2;
	for (--year; year >= EPOCH_YEAR; --year)
		tval += isleap(year) ?
		    DAYSPERLYEAR : DAYSPERNYEAR;
	while (--month)
		tval += dmsize[month];
	tval += day;
	tval = tval * HOURSPERDAY * MINSPERHOUR * SECSPERMIN;
	tval -= lt->tm_gmtoff;
	*store = tval;
	return(0);
}

char *
ok_shell(name)
	register char *name;
{
	register char *p, *sh;
	char *getusershell();

	setusershell();
	while (sh = getusershell()) {
		if (!strcmp(name, sh))
			return(name);
		/* allow just shell name, but use "real" path */
		if ((p = rindex(sh, '/')) && !strcmp(name, p + 1))
			return(sh);
	}
	return(NULL);
}
