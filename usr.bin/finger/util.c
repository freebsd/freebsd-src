/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)util.c	5.14 (Berkeley) 1/17/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <paths.h>
#include "finger.h"

find_idle_and_ttywrite(w)
	register WHERE *w;
{
	extern time_t now;
	extern int errno;
	struct stat sb;
	char *strerror();

	(void)sprintf(tbuf, "%s/%s", _PATH_DEV, w->tty);
	if (stat(tbuf, &sb) < 0) {
		(void)fprintf(stderr,
		    "finger: %s: %s\n", tbuf, strerror(errno));
		return;
	}
	w->idletime = now < sb.st_atime ? 0 : now - sb.st_atime;

#define	TALKABLE	0220		/* tty is writable if 220 mode */
	w->writable = ((sb.st_mode & TALKABLE) == TALKABLE);
}

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

match(pw, user)
	struct passwd *pw;
	char *user;
{
	register char *p, *t;
	char name[1024];

	/* why do we skip asterisks!?!? */
	(void)strcpy(p = tbuf, pw->pw_gecos);
	if (*p == '*')
		++p;

	/* ampersands get replaced by the login name */
	if (!(p = strtok(p, ",")))
		return(0);
	for (t = name; *t = *p; ++p)
		if (*t == '&') {
			(void)strcpy(t, pw->pw_name);
			while (*++t);
		}
		else
			++t;
	for (t = name; p = strtok(t, "\t "); t = (char *)NULL)
		if (!strcasecmp(p, user))
			return(1);
	return(0);
}

enter_lastlog(pn)
	register PERSON *pn;
{
	register WHERE *w;
	static int opened, fd;
	struct lastlog ll;
	char doit = 0;
	off_t lseek();

	/* some systems may not maintain lastlog, don't report errors. */
	if (!opened) {
		fd = open(_PATH_LASTLOG, O_RDONLY, 0);
		opened = 1;
	}
	if (fd == -1 ||
	    lseek(fd, (long)pn->uid * sizeof(ll), L_SET) !=
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

enter_where(ut, pn)
	struct utmp *ut;
	PERSON *pn;
{
	register WHERE *w = walloc(pn);

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
	register PERSON *pn, **pp;

	for (pp = htab + hash(pw->pw_name);
	     *pp != NULL && strcmp((*pp)->name, pw->pw_name) != 0;
	     pp = &(*pp)->hlink)
		;
	if ((pn = *pp) == NULL) {
		pn = palloc();
		entries++;
		if (phead == NULL)
			phead = ptail = pn;
		else {
			ptail->next = pn;
			ptail = pn;
		}
		pn->next = NULL;
		pn->hlink = NULL;
		*pp = pn;
		userinfo(pn, pw);
		pn->whead = NULL;
	}
	return(pn);
}

PERSON *
find_person(name)
	char *name;
{
	register PERSON *pn;

	/* name may be only UT_NAMESIZE long and not terminated */
	for (pn = htab[hash(name)];
	     pn != NULL && strncmp(pn->name, name, UT_NAMESIZE) != 0;
	     pn = pn->hlink)
		;
	return(pn);
}

hash(name)
	register char *name;
{
	register int h, i;

	h = 0;
	/* name may be only UT_NAMESIZE long and not terminated */
	for (i = UT_NAMESIZE; --i >= 0 && *name;)
		h = ((h << 2 | h >> HBITS - 2) ^ *name++) & HMASK;
	return(h);
}

PERSON *
palloc()
{
	PERSON *p;

	if ((p = (PERSON *)malloc((u_int) sizeof(PERSON))) == NULL) {
		(void)fprintf(stderr, "finger: out of space.\n");
		exit(1);
	}
	return(p);
}

WHERE *
walloc(pn)
	register PERSON *pn;
{
	register WHERE *w;

	if ((w = (WHERE *)malloc((u_int) sizeof(WHERE))) == NULL) {
		(void)fprintf(stderr, "finger: out of space.\n");
		exit(1);
	}
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
