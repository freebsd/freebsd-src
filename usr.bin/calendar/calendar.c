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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)calendar.c	8.3 (Berkeley) 3/25/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tzfile.h>
#include <unistd.h>

#include "pathnames.h"

struct passwd *pw;
int doall;

void	 cal __P((void));
void	 closecal __P((FILE *));
int	 getday __P((char *));
int	 getfield __P((char *, char **, int *));
int	 getmonth __P((char *));
int	 isnow __P((char *));
FILE	*opencal __P((void));
void	 settime __P((void));
void	 usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	int ch;

	while ((ch = getopt(argc, argv, "-a")) != EOF)
		switch (ch) {
		case '-':		/* backward contemptible */
		case 'a':
			if (getuid()) {
				errno = EPERM;
				err(1, NULL);
			}
			doall = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	settime();
	if (doall)
		while ((pw = getpwent()) != NULL) {
			(void)setegid(pw->pw_gid);
			(void)seteuid(pw->pw_uid);
			if (!chdir(pw->pw_dir))
				cal();
			(void)seteuid(0);
		}
	else
		cal();
	exit(0);
}

void
cal()
{
	register int printing;
	register char *p;
	FILE *fp;
	int ch;
	char buf[2048 + 1];

	if ((fp = opencal()) == NULL)
		return;
	for (printing = 0; fgets(buf, sizeof(buf), stdin) != NULL;) {
		if ((p = strchr(buf, '\n')) != NULL)
			*p = '\0';
		else
			while ((ch = getchar()) != '\n' && ch != EOF);
		if (buf[0] == '\0')
			continue;
		if (buf[0] != '\t')
			printing = isnow(buf) ? 1 : 0;
		if (printing)
			(void)fprintf(fp, "%s\n", buf);
	}
	closecal(fp);
}

struct iovec header[] = {
	"From: ", 6,
	NULL, 0,
	" (Reminder Service)\nTo: ", 24,
	NULL, 0,
	"\nSubject: ", 10,
	NULL, 0,
	"'s Calendar\nPrecedence: bulk\n\n",  30,
};

/* 1-based month, 0-based days, cumulative */
int daytab[][14] = {
	0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364,
	0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365,
};
struct tm *tp;
int *cumdays, offset, yrdays;
char dayname[10];

void
settime()
{
	time_t now;

	(void)time(&now);
	tp = localtime(&now);
	if (isleap(tp->tm_year + 1900)) {
		yrdays = DAYSPERLYEAR;
		cumdays = daytab[1];
	} else {
		yrdays = DAYSPERNYEAR;
		cumdays = daytab[0];
	}
	/* Friday displays Monday's events */
	offset = tp->tm_wday == 5 ? 3 : 1;
	header[5].iov_base = dayname;
	header[5].iov_len = strftime(dayname, sizeof(dayname), "%A", tp);
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
isnow(endp)
	char *endp;
{
	int day, flags, month, v1, v2;

#define	F_ISMONTH	0x01
#define	F_ISDAY		0x02
	flags = 0;
	/* didn't recognize anything, skip it */
	if (!(v1 = getfield(endp, &endp, &flags)))
		return (0);
	if (flags & F_ISDAY || v1 > 12) {
		/* found a day */
		day = v1;
		/* if no recognizable month, assume just a day alone */
		if (!(month = getfield(endp, &endp, &flags)))
			month = tp->tm_mon + 1;
	} else if (flags & F_ISMONTH) {
		month = v1;
		/* if no recognizable day, assume the first */
		if (!(day = getfield(endp, &endp, &flags)))
			day = 1;
	} else {
		v2 = getfield(endp, &endp, &flags);
		if (flags & F_ISMONTH) {
			day = v1;
			month = v2;
		} else {
			/* F_ISDAY set, v2 > 12, or no way to tell */
			month = v1;
			/* if no recognizable day, assume the first */
			day = v2 ? v2 : 1;
		}
	}
	if (flags & F_ISDAY)
		day = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);
	day = cumdays[month] + day;

	/* if today or today + offset days */
	if (day >= tp->tm_yday && day <= tp->tm_yday + offset)
		return (1);
	/* if number of days left in this year + days to event in next year */
	if (yrdays - tp->tm_yday + day <= offset)
		return (1);
	return (0);
}

int
getfield(p, endp, flags)
	char *p, **endp;
	int *flags;
{
	int val;
	char *start, savech;

	for (; !isdigit(*p) && !isalpha(*p) && *p != '*'; ++p);
	if (*p == '*') {			/* `*' is current month */
		*flags |= F_ISMONTH;
		*endp = p+1;
		return (tp->tm_mon + 1);
	}
	if (isdigit(*p)) {
		val = strtol(p, &p, 10);	/* if 0, it's failure */
		for (; !isdigit(*p) && !isalpha(*p) && *p != '*'; ++p);
		*endp = p;
		return (val);
	}
	for (start = p; isalpha(*++p););
	savech = *p;
	*p = '\0';
	if ((val = getmonth(start)) != 0)
		*flags |= F_ISMONTH;
	else if ((val = getday(start)) != 0)
		*flags |= F_ISDAY;
	else {
		*p = savech;
		return (0);
	}
	for (*p = savech; !isdigit(*p) && !isalpha(*p) && *p != '*'; ++p);
	*endp = p;
	return (val);
}

char path[MAXPATHLEN + 1];

FILE *
opencal()
{
	int fd, pdes[2];

	/* open up calendar file as stdin */
	if (!freopen("calendar", "r", stdin)) {
		if (doall)
			return (NULL);
		errx(1, "no calendar file.");
	}
	if (pipe(pdes) < 0) 
		return (NULL);
	switch (vfork()) {
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
		execl(_PATH_CPP, "cpp", "-I.", _PATH_INCLUDE, NULL);
		(void)fprintf(stderr,
		    "calendar: execl: %s: %s.\n", _PATH_CPP, strerror(errno));
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
closecal(fp)
	FILE *fp;
{
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
	switch (vfork()) {
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
		execl(_PATH_SENDMAIL, "sendmail", "-i", "-t", "-F",
		    "\"Reminder Service\"", "-f", "root", NULL);
		(void)fprintf(stderr,
		    "calendar: %s: %s.\n", _PATH_SENDMAIL, strerror(errno));
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

static char *months[] = {
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec", NULL,
};

int
getmonth(s)
	register char *s;
{
	register char **p;

	for (p = months; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - months) + 1);
	return (0);
}

static char *days[] = {
	"sun", "mon", "tue", "wed", "thu", "fri", "sat", NULL,
};

int
getday(s)
	register char *s;
{
	register char **p;

	for (p = days; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - days) + 1);
	return (0);
}

void
usage()
{
	(void)fprintf(stderr, "usage: calendar [-a]\n");
	exit(1);
}
