/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
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
static char sccsid[] = "@(#)util.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <db.h>
#include <pwd.h>
#include <utmp.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include "finger.h"

static void	 find_idle_and_ttywrite __P((WHERE *));
static void	 userinfo __P((PERSON *, struct passwd *));
static WHERE	*walloc __P((PERSON *));

int
match(pw, user)
	struct passwd *pw;
	char *user;
{
	register char *p, *t;
	char name[1024];

	if (!strcasecmp(pw->pw_name, user))
		return(1);

	/*
	 * XXX
	 * Why do we skip asterisks!?!?
	 */
	(void)strcpy(p = tbuf, pw->pw_gecos);
	if (*p == '*')
		++p;

	/* Ampersands get replaced by the login name. */
	if ((p = strtok(p, ",")) == NULL)
		return(0);

	for (t = name; *t = *p; ++p)
		if (*t == '&') {
			(void)strcpy(t, pw->pw_name);
			while (*++t);
		}
		else
			++t;
	for (t = name; p = strtok(t, "\t "); t = NULL)
		if (!strcasecmp(p, user))
			return(1);
	return(0);
}

void
enter_lastlog(pn)
	register PERSON *pn;
{
	register WHERE *w;
	static int opened, fd;
	struct lastlog ll;
	char doit = 0;

	/* some systems may not maintain lastlog, don't report errors. */
	if (!opened) {
		fd = open(_PATH_LASTLOG, O_RDONLY, 0);
		opened = 1;
	}
	if (fd == -1 ||
	    lseek(fd, (long)pn->uid * sizeof(ll), SEEK_SET) !=
	    (long)pn->uid * sizeof(ll) ||
	    read(fd, (char *)&ll, sizeof(ll)) != sizeof(ll)) {
			/* as if never logged in */
			ll.ll_line[0] = ll.ll_host[0] = NULL;
			ll.ll_time = 0;
		}
	if ((w = pn->whead) == NULL)
		doit = 1;
	else if (ll.ll_time != 0) {
		/* if last login is earlier than some current login */
		for (; !doit && w != NULL; w = w->next)
			if (w->info == LOGGEDIN && w->loginat < ll.ll_time)
				doit = 1;
		/*
		 * and if it's not any of the current logins
		 * can't use time comparison because there may be a small
		 * discrepency since login calls time() twice
		 */
		for (w = pn->whead; doit && w != NULL; w = w->next)
			if (w->info == LOGGEDIN &&
			    strncmp(w->tty, ll.ll_line, UT_LINESIZE) == 0)
				doit = 0;
	}
	if (doit) {
		w = walloc(pn);
		w->info = LASTLOG;
		bcopy(ll.ll_line, w->tty, UT_LINESIZE);
		w->tty[UT_LINESIZE] = 0;
		bcopy(ll.ll_host, w->host, UT_HOSTSIZE);
		w->host[UT_HOSTSIZE] = 0;
		w->loginat = ll.ll_time;
	}
}

void
enter_where(ut, pn)
	struct utmp *ut;
	PERSON *pn;
{
	register WHERE *w;

	w = walloc(pn);
	w->info = LOGGEDIN;
	bcopy(ut->ut_line, w->tty, UT_LINESIZE);
	w->tty[UT_LINESIZE] = 0;
	bcopy(ut->ut_host, w->host, UT_HOSTSIZE);
	w->host[UT_HOSTSIZE] = 0;
	w->loginat = (time_t)ut->ut_time;
	find_idle_and_ttywrite(w);
}

PERSON *
enter_person(pw)
	register struct passwd *pw;
{
	DBT data, key;
	PERSON *pn;

	if (db == NULL &&
	    (db = dbopen(NULL, O_RDWR, 0, DB_BTREE, NULL)) == NULL)
		err("%s", strerror(errno));

	key.data = pw->pw_name;
	key.size = strlen(pw->pw_name);

	switch((*db->get)(db, &key, &data, 0)) {
	case 0:
		return(*(PERSON **)data.data);
	default:
	case -1:
		err("db get: %s", strerror(errno));
		/* NOTREACHED */
	case 1:
		++entries;
		pn = palloc();
		userinfo(pn, pw);
		pn->whead = NULL;

		data.size = sizeof(PERSON *);
		data.data = &pn;
		if ((*db->put)(db, &key, &data, 0))
			err("%s", strerror(errno));
		return(pn);
	}
}

PERSON *
find_person(name)
	char *name;
{
	register int cnt;
	DBT data, key;
	char buf[UT_NAMESIZE + 1];

	if (!db)
		return(NULL);

	/* Name may be only UT_NAMESIZE long and not NUL terminated. */
	for (cnt = 0; cnt < UT_NAMESIZE && *name; ++name, ++cnt)
		buf[cnt] = *name;
	buf[cnt] = '\0';
	key.data = buf;
	key.size = cnt;

	return((*db->get)(db, &key, &data, 0) ? NULL : *(PERSON **)data.data);
}

PERSON *
palloc()
{
	PERSON *p;

	if ((p = malloc((u_int) sizeof(PERSON))) == NULL)
		err("%s", strerror(errno));
	return(p);
}

static WHERE *
walloc(pn)
	register PERSON *pn;
{
	register WHERE *w;

	if ((w = malloc((u_int) sizeof(WHERE))) == NULL)
		err("%s", strerror(errno));
	if (pn->whead == NULL)
		pn->whead = pn->wtail = w;
	else {
		pn->wtail->next = w;
		pn->wtail = w;
	}
	w->next = NULL;
	return(w);
}

char *
prphone(num)
	char *num;
{
	register char *p;
	int len;
	static char pbuf[15];

	/* don't touch anything if the user has their own formatting */
	for (p = num; *p; ++p)
		if (!isdigit(*p))
			return(num);
	len = p - num;
	p = pbuf;
	switch(len) {
	case 11:			/* +0-123-456-7890 */
		*p++ = '+';
		*p++ = *num++;
		*p++ = '-';
		/* FALLTHROUGH */
	case 10:			/* 012-345-6789 */
		*p++ = *num++;
		*p++ = *num++;
		*p++ = *num++;
		*p++ = '-';
		/* FALLTHROUGH */
	case 7:				/* 012-3456 */
		*p++ = *num++;
		*p++ = *num++;
		*p++ = *num++;
		break;
	case 5:				/* x0-1234 */
		*p++ = 'x';
		*p++ = *num++;
		break;
	default:
		return(num);
	}
	*p++ = '-';
	*p++ = *num++;
	*p++ = *num++;
	*p++ = *num++;
	*p++ = *num++;
	*p = '\0';
	return(pbuf);
}

static void
find_idle_and_ttywrite(w)
	register WHERE *w;
{
	extern time_t now;
	struct stat sb;

	(void)snprintf(tbuf, sizeof(tbuf), "%s/%s", _PATH_DEV, w->tty);
	if (stat(tbuf, &sb) < 0) {
		(void)fprintf(stderr,
		    "finger: %s: %s\n", tbuf, strerror(errno));
		return;
	}
	w->idletime = now < sb.st_atime ? 0 : now - sb.st_atime;

#define	TALKABLE	0220		/* tty is writable if 220 mode */
	w->writable = ((sb.st_mode & TALKABLE) == TALKABLE);
}

static void
userinfo(pn, pw)
	register PERSON *pn;
	register struct passwd *pw;
{
	register char *p, *t;
	char *bp, name[1024];

	pn->realname = pn->office = pn->officephone = pn->homephone = NULL;

	pn->uid = pw->pw_uid;
	pn->name = strdup(pw->pw_name);
	pn->dir = strdup(pw->pw_dir);
	pn->shell = strdup(pw->pw_shell);

	/* why do we skip asterisks!?!? */
	(void)strcpy(bp = tbuf, pw->pw_gecos);
	if (*bp == '*')
		++bp;

	/* ampersands get replaced by the login name */
	if (!(p = strsep(&bp, ",")))
		return;
	for (t = name; *t = *p; ++p)
		if (*t == '&') {
			(void)strcpy(t, pw->pw_name);
			if (islower(*t))
				*t = toupper(*t);
			while (*++t);
		}
		else
			++t;
	pn->realname = strdup(name);
	pn->office = ((p = strsep(&bp, ",")) && *p) ?
	    strdup(p) : NULL;
	pn->officephone = ((p = strsep(&bp, ",")) && *p) ?
	    strdup(p) : NULL;
	pn->homephone = ((p = strsep(&bp, ",")) && *p) ?
	    strdup(p) : NULL;
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "finger: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}
