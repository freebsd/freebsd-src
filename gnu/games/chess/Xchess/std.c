
/* This file contains code for X-CHESS.
   Copyright (C) 1986 Free Software Foundation, Inc.

This file is part of X-CHESS.

X-CHESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the X-CHESS General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
X-CHESS, but only under the conditions described in the
X-CHESS General Public License.   A copy of this license is
supposed to have been given to you along with X-CHESS so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


/* RCS Info: $Revision: 1.1.1.1 $ on $Date: 1993/06/12 14:41:07 $
 *           $Source: /a/cvs/386BSD/src/gnu/chess/Xchess/std.c,v $
 * Copyright (c) 1985 Wayne A. Christopher, U. C. Berkeley CAD Group
 *
 * Utility routines.
 */

#include "std.h"

#ifndef IBMPC
#include <sys/types.h>
#endif not IBMPC
#ifdef UNIX
#include <signal.h>
#include <pwd.h>
#endif UNIX
#ifdef BSD
#include <sys/time.h>
#include <sys/resource.h>
#endif BSD

extern char **environ;

bool
prefix(p, s)
	register char *p, *s;
{
	while (*p && (*p == *s))
		p++, s++;
	if (!*p)
		return (true);
	else
		return (false);
}

/* Create a copy of a string. */

char *
copy(str)
	char *str;
{
	char *p, *tmalloc();
	
	p = tmalloc(strlen(str) + 1);
	strcpy(p, str);
	return(p);
}

/* Determine whether sub is a substring of str. */

bool
substring(sub, str)
	register char *str, *sub;
{
	register char *s;

	while(*str) {
		if(*str == *sub) {
			for(s = sub; *s; s++)
				if(*s != *str++)
					break;
			if(*s == '\0')
				return (true);
		}
		str++;
	}
	return (false);
}

/* Malloc num bytes and initialize to zero. Fatal error if the space can't
 * be malloc'd. 
 */

char *
tmalloc(num)
	register int num;
{
	register char *s;
	char *malloc();

	s = malloc((unsigned) num);
	if (!s) {
		fatal("malloc: can't allocate %d bytes", num);
	}
	bzero(s, num);
	return(s);
}

char *
trealloc(ptr, num)
	char *ptr;
	int num;
{
	register char *s;
	char *realloc();

	s = realloc(ptr, (unsigned) num);
	if (!s) {
		fatal("realloc: can't allocate %d bytes", num);
	}
	/* Well, this won't be zeroed... Too bad... */
	return(s);
}

/* Append one character to a string. Don't check for overflow. */

void
appendc(s, c)
	char *s, c;
{
	while (*s)
		s++;
	*s++ = c;
	*s = '\0';
	return;
}

int
scannum(str)
	char *str;
{
	int i = 0;

	while(isdigit(*str))
		i = i * 10 + *(str++) - '0';
	return(i);
}

/* Case insensitive prefix. */

bool
ciprefix(p, s)
	register char *p, *s;
{
	while (*p) {
		if ((isupper(*p) ? tolower(*p) : *p) !=
		    (isupper(*s) ? tolower(*s) : *s))
			return(false);
		p++;
		s++;
	}
	return (true);
}

/* Case insensitive strcmp... */

bool
cieq(p, s)
	register char *p, *s;
{
	while (*p) {
		if ((isupper(*p) ? tolower(*p) : *p) !=
		    (isupper(*s) ? tolower(*s) : *s))
			return(false);
		p++;
		s++;
	}
	return (!*s);
}

#ifdef BSD

/* Return the date. Return value is static data. */

char *
datestring()
{
	register char *tzn;
	struct tm *tp;
	static char tbuf[40];
	char *ap;
	struct timeval tv;
	struct timezone tz;
	char *timezone(), *asctime();
	int i;
	struct tm *localtime();

	(void) gettimeofday(&tv, &tz);
	tp = localtime((time_t *) &tv.tv_sec);
	ap = asctime(tp);
	tzn = timezone(tz.tz_minuteswest, tp->tm_isdst);
	sprintf(tbuf, "%.20s", ap);
	if (tzn)
		strcat(tbuf, tzn);
	strcat(tbuf, ap + 19);
	i = strlen(tbuf);
	tbuf[i - 1] = '\0';
	return (tbuf);
}

#else BSD

/* Give it a try... */

char *
datestring()
{
	long i;
	static char buf[64];

	i = time(0);
	strcpy(buf, ctime(&i));
	buf[strlen(buf) - 1] = '\0';	/* Kill the nl. */
	return (buf);
}

#endif

/* How many seconds have elapsed in running time. */

int
seconds()
{
#ifdef BSD
	struct rusage ruse;

	getrusage(RUSAGE_SELF, &ruse);
	return (ruse.ru_utime.tv_sec);
#else BSD
#endif BSD
}

/* A few things that may not exist on non-unix systems. */

#ifndef BSD

#ifndef index

char *
index(s, c)
	register char *s;
	register char c;
{
	while ((*s != c) && (*s != '\0'))
		s++;
	if (*s == '\0')
		return ((char *) 0);
	else
		return (s);
}

#endif not index

#ifndef rindex

char *
rindex(s, c)
	register char *s;
	register char c;
{
	register char *t;

	for (t = s; *t != '\0'; t++);
	while ((*t != c) && (t != s))
		t--;
	if (t == s)
		return ((char *) 0);
	else
		return (t);
}

#endif not rindex

#ifndef bcopy

void
bcopy(from, to, num)
	register char *from, *to;
	register int num;
{
	while (num-- > 0)
		*to++ = *from++;
	return;
}

#endif not bcopy

#ifndef bzero

void
bzero(ptr, num)
	register char *ptr;
	register int num;
{
	while (num-- > 0)
		*ptr++ = '\0';
	return;
}

#endif not bzero

/* This might not be around... If not then forget about sorting... */

void qsort() {}

#endif BSD

char *
gettok(s)
	char **s;
{
	char buf[BSIZE];
	int i = 0;

	while (isspace(**s))
		(*s)++;
	if (!**s)
		return (NULL);
	while (**s && !isspace(**s))
		buf[i++] = *(*s)++;
	buf[i] = '\0';
	while (isspace(**s))
		(*s)++;
	return (copy(buf));
}

/* Die horribly. */

/* VARARGS1 */
void
fatal(s, args)
        char *s;
{
	fputs("Internal Error: ", stderr);
#ifndef __386BSD__
	_doprnt(s, &args, stderr);
#endif
	putc('\n', stderr);

	kill(getpid(), SIGIOT);
	/* NOTREACHED */
}

void
setenv(name, value)
	char *name, *value;
{
	int i;
	char **xx, *s;

	s = tmalloc(strlen(name) + 2);
	sprintf(s, "%s=", name);

	/* Copy the old environment... */
	for (i = 0; environ[i]; i++)
		if (prefix(s, environ[i]))
			break;
	if (!environ[i]) {
		xx = (char **) tmalloc((i + 2) * sizeof (char *));
		for (i = 0; environ[i]; i++)
			xx[i] = environ[i];
		xx[i + 1] = NULL;
		environ = xx;
	} else
		xx = environ;
	
	xx[i] = tmalloc(strlen(name) + strlen(value) + 2);
	sprintf(xx[i], "%s=%s", name, value);
	return;
}

char *
getusername()
{
	int i = getuid();
	struct passwd *pw = getpwuid(i);

	return (pw ? pw->pw_name : NULL);
}

char *
gethome()
{
	int i = getuid();
	struct passwd *pw = getpwuid(i);

	return (pw ? pw->pw_dir : "/strange");
}

char *
tildexpand(s)
	char *s;
{
	struct passwd *pw;
	char *n, buf[64];
	int i;

	if (*s != '~')
		return (copy(s));

	for (s++, i = 0; *s != '/'; s++, i++)
		buf[i] = *s;
	buf[i] = '\0';
	if (!i)
		pw = getpwuid(getuid());
	else
		pw = getpwnam(buf);
	if (!pw)
		return (s);
	n = tmalloc(strlen(s) + strlen(pw->pw_dir) + 1);
	strcpy(n, pw->pw_dir);
	strcat(n, s);
	return (n);
}

