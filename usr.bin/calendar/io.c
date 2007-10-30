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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#if 0
#ifndef lint
static char sccsid[] = "@(#)calendar.c  8.3 (Berkeley) 3/25/94";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"
#include "calendar.h"


const char *calendarFile = "calendar";  /* default calendar file */
const char *calendarHomes[] = { ".calendar", _PATH_INCLUDE }; /* HOME */
const char *calendarNoMail = "nomail";  /* don't sent mail if this file exist */

struct fixs neaster, npaskha;

struct iovec header[] = {
	{"From: ", 6},
	{NULL, 0},
	{" (Reminder Service)\nTo: ", 24},
	{NULL, 0},
	{"\nSubject: ", 10},
	{NULL, 0},
	{"'s Calendar\nPrecedence: bulk\n\n",  30},
};


void
cal(void)
{
	int printing;
	char *p;
	FILE *fp;
	int ch, l;
	int month;
	int day;
	int var;
	static int d_first = -1;
	char buf[2048 + 1];
	struct event *events = NULL;

	if ((fp = opencal()) == NULL)
		return;
	for (printing = 0; fgets(buf, sizeof(buf), stdin) != NULL;) {
		if ((p = strchr(buf, '\n')) != NULL)
			*p = '\0';
		else
			while ((ch = getchar()) != '\n' && ch != EOF);
		for (l = strlen(buf);
		     l > 0 && isspace((unsigned char)buf[l - 1]);
		     l--)
			;
		buf[l] = '\0';
		if (buf[0] == '\0')
			continue;
		if (strncmp(buf, "LANG=", 5) == 0) {
			(void) setlocale(LC_ALL, buf + 5);
			d_first = (*nl_langinfo(D_MD_ORDER) == 'd');
			setnnames();
			continue;
		}
		if (strncasecmp(buf, "Easter=", 7) == 0 && buf[7]) {
			if (neaster.name != NULL)
				free(neaster.name);
			if ((neaster.name = strdup(buf + 7)) == NULL)
				errx(1, "cannot allocate memory");
			neaster.len = strlen(buf + 7);
			continue;
		}
		if (strncasecmp(buf, "Paskha=", 7) == 0 && buf[7]) {
			if (npaskha.name != NULL)
				free(npaskha.name);
			if ((npaskha.name = strdup(buf + 7)) == NULL)
				errx(1, "cannot allocate memory");
			npaskha.len = strlen(buf + 7);
			continue;
		}
		if (buf[0] != '\t') {
			printing = isnow(buf, &month, &day, &var) ? 1 : 0;
			if ((p = strchr(buf, '\t')) == NULL)
				continue;
			if (p > buf && p[-1] == '*')
				var = 1;
			if (printing) {
				struct tm tm;
				char dbuf[80];

				if (d_first < 0)
					d_first = (*nl_langinfo(D_MD_ORDER) == 'd');
				tm.tm_sec = 0;  /* unused */
				tm.tm_min = 0;  /* unused */
				tm.tm_hour = 0; /* unused */
				tm.tm_wday = 0; /* unused */
				tm.tm_mon = month - 1;
				tm.tm_mday = day;
				tm.tm_year = tp->tm_year; /* unused */
				(void)strftime(dbuf, sizeof(dbuf),
					       d_first ? "%e %b" : "%b %e",
					       &tm);
				events = event_add(events, month, day, dbuf, var, p);
			}
		}
		else if (printing)
			event_continue(events, buf);
	}

	event_print_all(fp, events);
	closecal(fp);
}

/*
 * Functions to handle buffered calendar events.
 */
struct event *
event_add(struct event *events, int month, int day, char *date, int var, char *txt)
{
	struct event *e;

	e = (struct event *)calloc(1, sizeof(struct event));
	if (e == NULL)
		errx(1, "event_add: cannot allocate memory");
	e->month = month;
	e->day = day;
	e->var = var;
	e->date = strdup(date);
	if (e->date == NULL)
		errx(1, "event_add: cannot allocate memory");
	e->text = strdup(txt);
	if (e->text == NULL)
		errx(1, "event_add: cannot allocate memory");
	e->next = events;

	return e;
}

void
event_continue(struct event *e, char *txt)
{
	char *text;

	text = strdup(e->text);
	if (text == NULL)
		errx(1, "event_continue: cannot allocate memory");

	free(e->text);
	e->text = (char *)malloc(strlen(text) + strlen(txt) + 3);
	if (e->text == NULL)
		errx(1, "event_continue: cannot allocate memory");
	strcpy(e->text, text);
	strcat(e->text, "\n");
	strcat(e->text, txt);
	free(text);

	return;
}

void
event_print_all(FILE *fp, struct event *events)
{
	struct event *e, *e_next;
	int daycount = f_dayAfter + f_dayBefore;
	int daycounter;
	int day, month;

	for (daycounter = 0; daycounter <= daycount; daycounter++) {
		day = tp->tm_yday - f_dayBefore + daycounter;
		if (day < 0) day += yrdays;
		if (day >= yrdays) day -= yrdays;

		month = 1;
		while (month <= 12) {
			if (day <= cumdays[month])
				break;
			month++;
		}
		month--;
		day -= cumdays[month];

#ifdef DEBUG
		fprintf(stderr,"event_print_allmonth: %d, day: %d\n",month,day);
#endif

		for (e = events; e != NULL; e = e_next ) {
			e_next = e->next;

			if (month != e->month || day != e->day)
				continue;

			(void)fprintf(fp, "%s%c%s\n", e->date,
			    e->var ? '*' : ' ', e->text);
		}
	}
}

int
getfield(char *p, char **endp, int *flags)
{
	int val, var;
	char *start, savech;

	for (; !isdigit((unsigned char)*p) && !isalpha((unsigned char)*p)
               && *p != '*'; ++p);
	if (*p == '*') {			/* `*' is current month */
		*flags |= F_ISMONTH;
		*endp = p+1;
		return (tp->tm_mon + 1);
	}
	if (isdigit((unsigned char)*p)) {
		val = strtol(p, &p, 10);	/* if 0, it's failure */
		for (; !isdigit((unsigned char)*p)
                       && !isalpha((unsigned char)*p) && *p != '*'; ++p);
		*endp = p;
		return (val);
	}
	for (start = p; isalpha((unsigned char)*++p););

	/* Sunday-1 */
	if (*p == '+' || *p == '-')
	    for(; isdigit((unsigned char)*++p););

	savech = *p;
	*p = '\0';

	/* Month */
	if ((val = getmonth(start)) != 0)
		*flags |= F_ISMONTH;

	/* Day */
	else if ((val = getday(start)) != 0) {
	    *flags |= F_ISDAY;

	    /* variable weekday */
	    if ((var = getdayvar(start)) != 0) {
		if (var <=5 && var >= -4)
		    val += var * 10;
#ifdef DEBUG
		printf("var: %d\n", var);
#endif
	    }
	}

	/* Easter */
	else if ((val = geteaster(start, tp->tm_year + 1900)) != 0)
	    *flags |= F_EASTER;

	/* Paskha */
	else if ((val = getpaskha(start, tp->tm_year + 1900)) != 0)
	    *flags |= F_EASTER;

	/* undefined rest */
	else {
		*p = savech;
		return (0);
	}
	for (*p = savech; !isdigit((unsigned char)*p)
               && !isalpha((unsigned char)*p) && *p != '*'; ++p);
	*endp = p;
	return (val);
}

char path[MAXPATHLEN];

FILE *
opencal(void)
{
	uid_t uid;
	size_t i;
	int fd, found, pdes[2];
	struct stat sbuf;

	/* open up calendar file as stdin */
	if (!freopen(calendarFile, "r", stdin)) {
		if (doall) {
		    if (chdir(calendarHomes[0]) != 0)
			return (NULL);
		    if (stat(calendarNoMail, &sbuf) == 0)
		        return (NULL);
		    if (!freopen(calendarFile, "r", stdin))
		        return (NULL);
		} else {
			char *home = getenv("HOME");
			if (home == NULL || *home == '\0')
				errx(1, "cannot get home directory");
			chdir(home);
			for (found = i = 0; i < sizeof(calendarHomes) /
			    sizeof(calendarHomes[0]); i++)
			    if (chdir(calendarHomes[i]) == 0 &&
			          freopen(calendarFile, "r", stdin)) {
				    found = 1;
				    break;
			    }
			if (!found)
			    errx(1, "no calendar file: ``%s''", calendarFile);
		}
	}
	if (pipe(pdes) < 0)
		return (NULL);
	switch (fork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		return (NULL);
	case 0:
		/* child -- stdin already setup, set stdout to pipe input */
		if (pdes[1] != STDOUT_FILENO) {
			(void)dup2(pdes[1], STDOUT_FILENO);
			(void)close(pdes[1]);
		}
		(void)close(pdes[0]);
		uid = geteuid();
		if (setuid(getuid()) < 0) {
			warnx("first setuid failed");
			_exit(1);
		};
		if (setgid(getegid()) < 0) {
			warnx("setgid failed");
			_exit(1);
		}
		if (setuid(uid) < 0) {
			warnx("setuid failed");
			_exit(1);
		}
		execl(_PATH_CPP, "cpp", "-P",
		    "-traditional", "-nostdinc",	/* GCC specific opts */
		    "-I.", "-I", _PATH_INCLUDE, (char *)NULL);
		warn(_PATH_CPP);
		_exit(1);
	}
	/* parent -- set stdin to pipe output */
	(void)dup2(pdes[0], STDIN_FILENO);
	(void)close(pdes[0]);
	(void)close(pdes[1]);

	/* not reading all calendar files, just set output to stdout */
	if (!doall)
		return (stdout);

	/* set output to a temporary file, so if no output don't send mail */
	(void)snprintf(path, sizeof(path), "%s/_calXXXXXX", _PATH_TMP);
	if ((fd = mkstemp(path)) < 0)
		return (NULL);
	return (fdopen(fd, "w+"));
}

void
closecal(FILE *fp)
{
	uid_t uid;
	struct stat sbuf;
	int nread, pdes[2], status;
	char buf[1024];

	if (!doall)
		return;

	(void)rewind(fp);
	if (fstat(fileno(fp), &sbuf) || !sbuf.st_size)
		goto done;
	if (pipe(pdes) < 0)
		goto done;
	switch (fork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto done;
	case 0:
		/* child -- set stdin to pipe output */
		if (pdes[0] != STDIN_FILENO) {
			(void)dup2(pdes[0], STDIN_FILENO);
			(void)close(pdes[0]);
		}
		(void)close(pdes[1]);
		uid = geteuid();
		if (setuid(getuid()) < 0) {
			warnx("setuid failed");
			_exit(1);
		};
		if (setgid(getegid()) < 0) {
			warnx("setgid failed");
			_exit(1);
		}
		if (setuid(uid) < 0) {
			warnx("setuid failed");
			_exit(1);
		}
		execl(_PATH_SENDMAIL, "sendmail", "-i", "-t", "-F",
		    "\"Reminder Service\"", (char *)NULL);
		warn(_PATH_SENDMAIL);
		_exit(1);
	}
	/* parent -- write to pipe input */
	(void)close(pdes[0]);

	header[1].iov_base = header[3].iov_base = pw->pw_name;
	header[1].iov_len = header[3].iov_len = strlen(pw->pw_name);
	writev(pdes[1], header, 7);
	while ((nread = read(fileno(fp), buf, sizeof(buf))) > 0)
		(void)write(pdes[1], buf, nread);
	(void)close(pdes[1]);
done:	(void)fclose(fp);
	(void)unlink(path);
	while (wait(&status) >= 0);
}
