/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
static char sccsid[] = "@(#)attime.c	5.2 (Berkeley) 8/30/90";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

#define	HR	(60 * 60)
#define	DAY	(24 * HR)
#define	MON	(30 * DAY)

static time_t now;
/*
 * prttime prints a time in hours and minutes or minutes and seconds.
 * The character string tail is printed at the end, obvious
 * strings to pass are "", " ", or "am".
 */
static char *
prttime(tim, tail)
	time_t tim;
	char *tail;
{
	int mins;
	static char timebuf[32];

	if (tim >= 60) {
		mins = tim % 60;
		(void) sprintf(timebuf, "%2d:%02d%s", (int)(tim / 60), mins,
		    tail);
	} else if (tim >= 0)
		(void) sprintf(timebuf, "    %2d%s", (int)tim, tail);
	else
		(void) strcpy(timebuf, tail);

	return (timebuf);
}

static char *weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* prtat prints a 12 hour time given a pointer to a time of day */
char *
attime(started)
	time_t *started;
{
	struct tm *p;
	register int hr, pm;
	static char prbuff[64];

	if (now == 0)
		(void) time(&now);
	p = localtime(started);
	hr = p->tm_hour;
	pm = (hr > 11);
	if (hr > 11)
		hr -= 12;
	if (hr == 0)
		hr = 12;
	if (now - *started <= 18 * HR)
		return (prttime((time_t)hr * 60 + p->tm_min, pm ? "pm" : "am"));
	if (now - *started <= 7 * DAY)
		(void) sprintf(prbuff, "%*s%d%s", hr < 10 ? 4 : 3,
			weekday[p->tm_wday], hr, pm ? "pm" : "am");
	else
		(void) sprintf(prbuff, "%2d%s%2d", p->tm_mday,
			month[p->tm_mon], p->tm_year);

	return (prbuff);
}
