/*
 * Copyright (c) 1989, 1993, 1994
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/uio.h>
#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pathnames.h"
#include "calendar.h"

struct tm *tp;
static const struct tm tm0;
int *cumdays, yrdays;
char dayname[10];


/* 1-based month, 0-based days, cumulative */
int daytab[][14] = {
	{ 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364 },
	{ 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
};

static char const *days[] = {
	"sun", "mon", "tue", "wed", "thu", "fri", "sat", NULL,
};

static const char *months[] = {
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec", NULL,
};

static struct fixs fndays[8];         /* full national days names */
static struct fixs ndays[8];          /* short national days names */

static struct fixs fnmonths[13];      /* full national months names */
static struct fixs nmonths[13];       /* short national month names */


void
setnnames(void)
{
	char buf[80];
	int i, l;
	struct tm tm;

	for (i = 0; i < 7; i++) {
		tm.tm_wday = i;
		strftime(buf, sizeof(buf), "%a", &tm);
		for (l = strlen(buf);
		     l > 0 && isspace((unsigned char)buf[l - 1]);
		     l--)
			;
		buf[l] = '\0';
		if (ndays[i].name != NULL)
			free(ndays[i].name);
		if ((ndays[i].name = strdup(buf)) == NULL)
			errx(1, "cannot allocate memory");
		ndays[i].len = strlen(buf);

		strftime(buf, sizeof(buf), "%A", &tm);
		for (l = strlen(buf);
		     l > 0 && isspace((unsigned char)buf[l - 1]);
		     l--)
			;
		buf[l] = '\0';
		if (fndays[i].name != NULL)
			free(fndays[i].name);
		if ((fndays[i].name = strdup(buf)) == NULL)
			errx(1, "cannot allocate memory");
		fndays[i].len = strlen(buf);
	}

	for (i = 0; i < 12; i++) {
		tm.tm_mon = i;
		strftime(buf, sizeof(buf), "%b", &tm);
		for (l = strlen(buf);
		     l > 0 && isspace((unsigned char)buf[l - 1]);
		     l--)
			;
		buf[l] = '\0';
		if (nmonths[i].name != NULL)
			free(nmonths[i].name);
		if ((nmonths[i].name = strdup(buf)) == NULL)
			errx(1, "cannot allocate memory");
		nmonths[i].len = strlen(buf);

		strftime(buf, sizeof(buf), "%B", &tm);
		for (l = strlen(buf);
		     l > 0 && isspace((unsigned char)buf[l - 1]);
		     l--)
			;
		buf[l] = '\0';
		if (fnmonths[i].name != NULL)
			free(fnmonths[i].name);
		if ((fnmonths[i].name = strdup(buf)) == NULL)
			errx(1, "cannot allocate memory");
		fnmonths[i].len = strlen(buf);
	}
}

void
settime(time_t now)
{
	char *oldl, *lbufp;

	tp = localtime(&now);
	if ( isleap(tp->tm_year + 1900) ) {
		yrdays = 366;
		cumdays = daytab[1];
	} else {
		yrdays = 365;
		cumdays = daytab[0];
	}
	/* Friday displays Monday's events */
	if (f_dayAfter == 0 && f_dayBefore == 0 && Friday != -1)
		f_dayAfter = tp->tm_wday == Friday ? 3 : 1;
	header[5].iov_base = dayname;

	oldl = NULL;
	lbufp = setlocale(LC_TIME, NULL);
	if (lbufp != NULL && (oldl = strdup(lbufp)) == NULL)
		errx(1, "cannot allocate memory");
	(void) setlocale(LC_TIME, "C");
	header[5].iov_len = strftime(dayname, sizeof(dayname), "%A", tp);
	(void) setlocale(LC_TIME, (oldl != NULL ? oldl : ""));
	if (oldl != NULL)
		free(oldl);

	setnnames();
}

/* convert Day[/Month][/Year] into unix time (since 1970)
 * Day: two digits, Month: two digits, Year: digits
 */
time_t
Mktime (char *dp)
{
    time_t t;
    int d, m, y;
    struct tm tm;

    (void)time(&t);
    tp = localtime(&t);

    tm = tm0;
    tm.tm_mday = tp->tm_mday;
    tm.tm_mon = tp->tm_mon;
    tm.tm_year = tp->tm_year;

    switch (sscanf(dp, "%d.%d.%d", &d, &m, &y)) {
    case 3:
	if (y > 1900)
	    y -= 1900;
	tm.tm_year = y;
	/* FALLTHROUGH */
    case 2:
	tm.tm_mon = m - 1;
	/* FALLTHROUGH */
    case 1:
	tm.tm_mday = d;
    }

#ifdef DEBUG
    fprintf(stderr, "Mktime: %d %d %s\n", (int)mktime(&tm), (int)t,
	   asctime(&tm));
#endif
    return(mktime(&tm));
}

/*
 * Possible date formats include any combination of:
 *	3-charmonth			(January, Jan, Jan)
 *	3-charweekday			(Friday, Monday, mon.)
 *	numeric month or day		(1, 2, 04)
 *
 * Any character may separate them, or they may not be separated.  Any line,
 * following a line that is matched, that starts with "whitespace", is shown
 * along with the matched line.
 */
int
isnow(char *endp, int *monthp, int *dayp, int *varp)
{
	int day, flags, month = 0, v1, v2;

	/*
	 * CONVENTION
	 *
	 * Month:     1-12
	 * Monthname: Jan .. Dec
	 * Day:       1-31
	 * Weekday:   Mon-Sun
	 *
	 */

	flags = 0;

	/* read first field */
	/* didn't recognize anything, skip it */
	if (!(v1 = getfield(endp, &endp, &flags)))
		return (0);

	/* Easter or Easter depending days */
	if (flags & F_EASTER)
	    day = v1 - 1; /* days since January 1 [0-365] */

	 /*
	  * 1. {Weekday,Day} XYZ ...
	  *
	  *    where Day is > 12
	  */
	else if (flags & F_ISDAY || v1 > 12) {

		/* found a day; day: 1-31 or weekday: 1-7 */
		day = v1;

		/* {Day,Weekday} {Month,Monthname} ... */
		/* if no recognizable month, assume just a day alone
		 * in other words, find month or use current month */
		if (!(month = getfield(endp, &endp, &flags)))
			month = tp->tm_mon + 1;
	}

	/* 2. {Monthname} XYZ ... */
	else if (flags & F_ISMONTH) {
		month = v1;

		/* Monthname {day,weekday} */
		/* if no recognizable day, assume the first day in month */
		if (!(day = getfield(endp, &endp, &flags)))
			day = 1;
	}

	/* Hm ... */
	else {
		v2 = getfield(endp, &endp, &flags);

		/*
		 * {Day} {Monthname} ...
		 * where Day <= 12
		 */
		if (flags & F_ISMONTH) {
			day = v1;
			month = v2;
			*varp = 0;
		}

		/* {Month} {Weekday,Day} ...  */
		else {
			/* F_ISDAY set, v2 > 12, or no way to tell */
			month = v1;
			/* if no recognizable day, assume the first */
			day = v2 ? v2 : 1;
			*varp = 0;
		}
	}

	/* convert Weekday into *next*  Day,
	 * e.g.: 'Sunday' -> 22
	 *       'SundayLast' -> ??
	 */
	if (flags & F_ISDAY) {
#ifdef DEBUG
	    fprintf(stderr, "\nday: %d %s month %d\n", day, endp, month);
#endif

	    *varp = 1;
	    /* variable weekday, SundayLast, MondayFirst ... */
	    if (day < 0 || day >= 10) {

		/* negative offset; last, -4 .. -1 */
		if (day < 0) {
		    v1 = day/10 - 1;          /* offset -4 ... -1 */
	            day = 10 + (day % 10);    /* day 1 ... 7 */

		    /* day, eg '22nd' */
		    v2 = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);

		    /* (month length - day) / 7 + 1 */
		    if (cumdays[month+1] - cumdays[month] >= v2
			&& ((int)((cumdays[month+1] -
		               cumdays[month] - v2) / 7) + 1) == -v1)
			/* bingo ! */
			day = v2;

		    /* set to yesterday */
		    else {
			day = tp->tm_mday - 1;
			if (day == 0)
			    return (0);
		    }
		}

		/* first, second ... +1 ... +5 */
		else {
		    v1 = day/10;        /* offset: +1 (first Sunday) ... */
		    day = day % 10;

		    /* day, eg '22th' */
		    v2 = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);

		    /* Hurrah! matched */
		    if ( ((v2 - 1 + 7) / 7) == v1 )
			day = v2;

		    /* set to yesterday */
		    else {
			day = tp->tm_mday - 1;
			if (day == 0)
			    return (0);
		    }
		}
	    }

	    /* wired */
	    else {
		day = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);
		*varp = 1;
	    }
	}

	if (!(flags & F_EASTER)) {
	    if (day + cumdays[month] > cumdays[month + 1]) {    /* off end of month */
		day -= (cumdays[month + 1] - cumdays[month]);   /* adjust */
		if (++month > 12)                               /* next year */
		    month = 1;
	    }
	    *monthp = month;
	    *dayp = day;
	    day = cumdays[month] + day;
	}
	else {
	    for (v1 = 0; day > cumdays[v1]; v1++)
		;
	    *monthp = v1 - 1;
	    *dayp = day - cumdays[v1 - 1];
	    *varp = 1;
	}

#ifdef DEBUG
	fprintf(stderr, "day2: day %d(%d-%d) yday %d\n", *dayp, day,
                cumdays[month], tp->tm_yday);
#endif

	/* When days before or days after is specified */
	/* no year rollover */
	if (day >= tp->tm_yday - f_dayBefore &&
	    day <= tp->tm_yday + f_dayAfter)
		return (1);

	/* next year */
	if (tp->tm_yday + f_dayAfter >= yrdays) {
		int end = tp->tm_yday + f_dayAfter - yrdays;
		if (day <= end)
			return (1);
	}

	/* previous year */
	if (tp->tm_yday - f_dayBefore < 0) {
		int before = yrdays + (tp->tm_yday - f_dayBefore );
		if (day >= before)
			return (1);
	}

	return (0);
}


int
getmonth(char *s)
{
	const char **p;
	struct fixs *n;

	for (n = fnmonths; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - fnmonths) + 1);
	for (n = nmonths; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - nmonths) + 1);
	for (p = months; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - months) + 1);
	return (0);
}


int
getday(char *s)
{
	const char **p;
	struct fixs *n;

	for (n = fndays; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - fndays) + 1);
	for (n = ndays; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - ndays) + 1);
	for (p = days; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - days) + 1);
	return (0);
}

/* return offset for variable weekdays
 * -1 -> last weekday in month
 * +1 -> first weekday in month
 * ... etc ...
 */
int
getdayvar(char *s)
{
	int offs;


	offs = strlen(s);


	/* Sun+1 or Wednesday-2
	 *    ^              ^   */

	/* fprintf(stderr, "x: %s %s %d\n", s, s + offs - 2, offs); */
	switch(*(s + offs - 2)) {
	case '-':
	    return(-(atoi(s + offs - 1)));
	case '+':
	    return(atoi(s + offs - 1));
	}


	/*
	 * some aliases: last, first, second, third, fourth
	 */

	/* last */
	if      (offs > 4 && !strcasecmp(s + offs - 4, "last"))
	    return(-1);
	else if (offs > 5 && !strcasecmp(s + offs - 5, "first"))
	    return(+1);
	else if (offs > 6 && !strcasecmp(s + offs - 6, "second"))
	    return(+2);
	else if (offs > 5 && !strcasecmp(s + offs - 5, "third"))
	    return(+3);
	else if (offs > 6 && !strcasecmp(s + offs - 6, "fourth"))
	    return(+4);


	/* no offset detected */
	return(0);
}
