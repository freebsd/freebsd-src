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
#if 0
static char sccsid[] = "@(#)lprint.c	8.3 (Berkeley) 4/28/95";
#else
static const char rcsid[] =
	"$Id$";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <db.h>
#include <err.h>
#include <pwd.h>
#include <utmp.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <paths.h>
#include "finger.h"

#define	LINE_LEN	80
#define	TAB_LEN		8		/* 8 spaces between tabs */
#define	_PATH_FORWARD	".forward"
#define	_PATH_PLAN	".plan"
#define	_PATH_PROJECT	".project"

static int	demi_print __P((char *, int));
static void	lprint __P((PERSON *));
static int	show_text __P((char *, char *, char *));
static void	vputc __P((int));

void
lflag_print()
{
	extern int pplan;
	register PERSON *pn;
	register int sflag, r;
	PERSON *tmp;
	DBT data, key;

	for (sflag = R_FIRST;; sflag = R_NEXT) {
		r = (*db->seq)(db, &key, &data, sflag);
		if (r == -1)
			err(1, "db seq");
		if (r == 1)
			break;
		memmove(&tmp, data.data, sizeof tmp);
		pn = tmp;
		if (sflag != R_FIRST)
			putchar('\n');
		lprint(pn);
		if (!pplan) {
			(void)show_text(pn->dir,
			    _PATH_FORWARD, "Mail forwarded to");
			(void)show_text(pn->dir, _PATH_PROJECT, "Project");
			if (!show_text(pn->dir, _PATH_PLAN, "Plan"))
				(void)printf("No Plan.\n");
		}
	}
}

static void
lprint(pn)
	register PERSON *pn;
{
	extern time_t now;
	register struct tm *delta;
	register WHERE *w;
	register int cpr, len, maxlen;
	struct tm *tp;
	int oddfield;
	char *tzn;
	char t[80];

	/*
	 * long format --
	 *	login name
	 *	real name
	 *	home directory
	 *	shell
	 *	office, office phone, home phone if available
	 *	mail status
	 */
	(void)printf("Login: %-15s\t\t\tName: %s\nDirectory: %-25s",
	    pn->name, pn->realname, pn->dir);
	(void)printf("\tShell: %-s\n", *pn->shell ? pn->shell : _PATH_BSHELL);

	/*
	 * try and print office, office phone, and home phone on one line;
	 * if that fails, do line filling so it looks nice.
	 */
#define	OFFICE_TAG		"Office"
#define	OFFICE_PHONE_TAG	"Office Phone"
	oddfield = 0;
	if (pn->office && pn->officephone &&
	    strlen(pn->office) + strlen(pn->officephone) +
	    sizeof(OFFICE_TAG) + 2 <= 5 * TAB_LEN) {
		(void)snprintf(tbuf, sizeof(tbuf), "%s: %s, %s",
		    OFFICE_TAG, pn->office, prphone(pn->officephone));
		oddfield = demi_print(tbuf, oddfield);
	} else {
		if (pn->office) {
			(void)snprintf(tbuf, sizeof(tbuf), "%s: %s",
			    OFFICE_TAG, pn->office);
			oddfield = demi_print(tbuf, oddfield);
		}
		if (pn->officephone) {
			(void)snprintf(tbuf, sizeof(tbuf), "%s: %s",
			    OFFICE_PHONE_TAG, prphone(pn->officephone));
			oddfield = demi_print(tbuf, oddfield);
		}
	}
	if (pn->homephone) {
		(void)snprintf(tbuf, sizeof(tbuf), "%s: %s", "Home Phone",
		    prphone(pn->homephone));
		oddfield = demi_print(tbuf, oddfield);
	}
	if (oddfield)
		putchar('\n');

	/*
	 * long format con't:
	 * if logged in
	 *	terminal
	 *	idle time
	 *	if messages allowed
	 *	where logged in from
	 * if not logged in
	 *	when last logged in
	 */
	/* find out longest device name for this user for formatting */
	for (w = pn->whead, maxlen = -1; w != NULL; w = w->next)
		if ((len = strlen(w->tty)) > maxlen)
			maxlen = len;
	/* find rest of entries for user */
	for (w = pn->whead; w != NULL; w = w->next) {
		switch (w->info) {
		case LOGGEDIN:
			tp = localtime(&w->loginat);
			strftime(t, sizeof(t), "%c", tp);
			tzn = tp->tm_zone;
			cpr = printf("On since %.16s (%s) on %s",
			    t, tzn, w->tty);
			/*
			 * idle time is tough; if have one, print a comma,
			 * then spaces to pad out the device name, then the
			 * idle time.  Follow with a comma if a remote login.
			 */
			delta = gmtime(&w->idletime);
			if (delta->tm_yday || delta->tm_hour || delta->tm_min) {
				cpr += printf("%-*s idle ",
				    maxlen - strlen(w->tty) + 1, ",");
				if (delta->tm_yday > 0) {
					cpr += printf("%d day%s ",
					   delta->tm_yday,
					   delta->tm_yday == 1 ? "" : "s");
				}
				cpr += printf("%d:%02d",
				    delta->tm_hour, delta->tm_min);
				if (*w->host) {
					putchar(',');
					++cpr;
				}
			}
			if (!w->writable)
				cpr += printf(" (messages off)");
			break;
		case LASTLOG:
			if (w->loginat == 0) {
				(void)printf("Never logged in.");
				break;
			}
			tp = localtime(&w->loginat);
			strftime(t, sizeof(t), "%c", tp);
			tzn = tp->tm_zone;
			if (now - w->loginat > 86400 * 365 / 2)
				cpr =
				    printf("Last login %.16s %.4s (%s) on %s",
				    t, t + 20, tzn, w->tty);
			else
				cpr = printf("Last login %.16s (%s) on %s",
				    t, tzn, w->tty);
			break;
		}
		if (*w->host) {
			if (LINE_LEN < (cpr + 6 + strlen(w->host)))
				(void)printf("\n   ");
			(void)printf(" from %s", w->host);
		}
		putchar('\n');
	}
	if (pn->mailrecv == -1)
		printf("No Mail.\n");
	else if (pn->mailrecv > pn->mailread) {
		tp = localtime(&pn->mailrecv);
		strftime(t, sizeof(t), "%c", tp);
		tzn = tp->tm_zone;
		printf("New mail received %.16s %.4s (%s)\n", t, t + 20, tzn);
		tp = localtime(&pn->mailread);
		strftime(t, sizeof(t), "%c", tp);
		tzn = tp->tm_zone;
		printf("     Unread since %.16s %.4s (%s)\n", t, t + 20, tzn);
	} else {
		tp = localtime(&pn->mailread);
		strftime(t, sizeof(t), "%c", tp);
		tzn = tp->tm_zone;
		printf("Mail last read %.16s %.4s (%s)\n", t, t + 20, tzn);
	}
}

static int
demi_print(str, oddfield)
	char *str;
	int oddfield;
{
	static int lenlast;
	int lenthis, maxlen;

	lenthis = strlen(str);
	if (oddfield) {
		/*
		 * We left off on an odd number of fields.  If we haven't
		 * crossed the midpoint of the screen, and we have room for
		 * the next field, print it on the same line; otherwise,
		 * print it on a new line.
		 *
		 * Note: we insist on having the right hand fields start
		 * no less than 5 tabs out.
		 */
		maxlen = 5 * TAB_LEN;
		if (maxlen < lenlast)
			maxlen = lenlast;
		if (((((maxlen / TAB_LEN) + 1) * TAB_LEN) +
		    lenthis) <= LINE_LEN) {
			while(lenlast < (4 * TAB_LEN)) {
				putchar('\t');
				lenlast += TAB_LEN;
			}
			(void)printf("\t%s\n", str);	/* force one tab */
		} else {
			(void)printf("\n%s", str);	/* go to next line */
			oddfield = !oddfield;	/* this'll be undone below */
		}
	} else
		(void)printf("%s", str);
	oddfield = !oddfield;			/* toggle odd/even marker */
	lenlast = lenthis;
	return(oddfield);
}

static int
show_text(directory, file_name, header)
	char *directory, *file_name, *header;
{
	struct stat sb;
	register FILE *fp;
	register int ch, cnt, lastc;
	register char *p;
	int fd, nr;

	(void)snprintf(tbuf, sizeof(tbuf), "%s/%s", directory, file_name);
	if ((fd = open(tbuf, O_RDONLY)) < 0 || fstat(fd, &sb) ||
	    sb.st_size == 0)
		return(0);

	/* If short enough, and no newlines, show it on a single line.*/
	if (sb.st_size <= LINE_LEN - strlen(header) - 5) {
		nr = read(fd, tbuf, sizeof(tbuf));
		if (nr <= 0) {
			(void)close(fd);
			return(0);
		}
		for (p = tbuf, cnt = nr; cnt--; ++p)
			if (*p == '\n')
				break;
		if (cnt <= 1) {
			(void)printf("%s: ", header);
			for (p = tbuf, cnt = nr; cnt--; ++p)
				if (*p != '\r')
					vputc(lastc = *p);
			if (lastc != '\n')
				(void)putchar('\n');
			(void)close(fd);
			return(1);
		}
		else
			(void)lseek(fd, 0L, SEEK_SET);
	}
	if ((fp = fdopen(fd, "r")) == NULL)
		return(0);
	(void)printf("%s:\n", header);
	while ((ch = getc(fp)) != EOF)
		if (ch != '\r')
			vputc(lastc = ch);
	if (lastc != '\n')
		(void)putchar('\n');
	(void)fclose(fp);
	return(1);
}

static void
vputc(ch)
	register int ch;
{
	int meta;

	if (!isprint(ch) && !isascii(ch)) {
		(void)putchar('M');
		(void)putchar('-');
		ch = toascii(ch);
		meta = 1;
	} else
		meta = 0;
	if (isprint(ch) || !meta && (ch == ' ' || ch == '\t' || ch == '\n'))
		(void)putchar(ch);
	else {
		(void)putchar('^');
		(void)putchar(ch == '\177' ? '?' : ch | 0100);
	}
}
