/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
static char sccsid[] = "@(#)cd.c	8.2 (Berkeley) 5/4/95";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/*
 * The cd and pwd commands.
 */

#include "shell.h"
#include "var.h"
#include "nodes.h"	/* for jobs.h */
#include "jobs.h"
#include "options.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "redir.h"
#include "mystring.h"
#include "show.h"

STATIC int docd __P((char *, int));
STATIC char *getcomponent __P((void));
STATIC void updatepwd __P((char *));
STATIC void getpwd __P((void));

char *curdir;			/* current working directory */
char *prevdir;			/* previous working directory */
STATIC char *cdcomppath;

int
cdcmd(argc, argv)
	int argc;
	char **argv; 
{
	char *dest;
	char *path;
	char *p;
	struct stat statb;
	char *padvance();
	int print = 0;

	nextopt(nullstr);
	if ((dest = *argptr) == NULL && (dest = bltinlookup("HOME", 1)) == NULL)
		error("HOME not set");
	if (dest[0] == '-' && dest[1] == '\0') {
		dest = prevdir ? prevdir : curdir;
		print = 1;
	}
	if (*dest == '/' || (path = bltinlookup("CDPATH", 1)) == NULL)
		path = nullstr;
	while ((p = padvance(&path, dest)) != NULL) {
		if (stat(p, &statb) >= 0 && S_ISDIR(statb.st_mode)) {
			if (!print) {
				/*
				 * XXX - rethink
				 */
				if (p[0] == '.' && p[1] == '/')
					p += 2;
				print = strcmp(p, dest);
			}
			if (docd(p, print) >= 0)
				return 0;

		}
	}
	error("can't cd to %s", dest);
	/*NOTREACHED*/
	return 0;
}


/*
 * Actually do the chdir.  If the name refers to symbolic links, we
 * compute the actual directory name before doing the cd.  In an
 * interactive shell, print the directory name if "print" is nonzero
 * or if the name refers to a symbolic link.  We also print the name
 * if "/u/logname" was expanded in it, since this is similar to a
 * symbolic link.  (The check for this breaks if the user gives the
 * cd command some additional, unused arguments.)
 */

#if SYMLINKS == 0
STATIC int
docd(dest, print)
	char *dest;
	{
	INTOFF;
	if (chdir(dest) < 0) {
		INTON;
		return -1;
	}
	updatepwd(dest);
	INTON;
	if (print && iflag)
		out1fmt("%s\n", stackblock());
	return 0;
}

#else



STATIC int
docd(dest, print)
	char *dest;
	int print;
{
	register char *p;
	register char *q;
	char *symlink;
	char *component;
	struct stat statb;
	int first;
	int i;

	TRACE(("docd(\"%s\", %d) called\n", dest, print));

top:
	cdcomppath = dest;
	STARTSTACKSTR(p);
	if (*dest == '/') {
		STPUTC('/', p);
		cdcomppath++;
	}
	first = 1;
	while ((q = getcomponent()) != NULL) {
		if (q[0] == '\0' || (q[0] == '.' && q[1] == '\0'))
			continue;
		if (! first)
			STPUTC('/', p);
		first = 0;
		component = q;
		while (*q)
			STPUTC(*q++, p);
		if (equal(component, ".."))
			continue;
		STACKSTRNUL(p);
		if (lstat(stackblock(), &statb) < 0)
			error("lstat %s failed", stackblock());
		if (!S_ISLNK(statb.st_mode))
			continue;

		/* Hit a symbolic link.  We have to start all over again. */
		print = 1;
		STPUTC('\0', p);
		symlink = grabstackstr(p);
		i = (int)statb.st_size + 2;		/* 2 for '/' and '\0' */
		if (cdcomppath != NULL)
			i += strlen(cdcomppath);
		p = stalloc(i);
		if (readlink(symlink, p, (int)statb.st_size) < 0) {
			error("readlink %s failed", stackblock());
		}
		if (cdcomppath != NULL) {
			p[(int)statb.st_size] = '/';
			scopy(cdcomppath, p + (int)statb.st_size + 1);
		} else {
			p[(int)statb.st_size] = '\0';
		}
		if (p[0] != '/') {	/* relative path name */
			char *r;
			q = r = symlink;
			while (*q) {
				if (*q++ == '/')
					r = q;
			}
			*r = '\0';
			dest = stalloc(strlen(symlink) + strlen(p) + 1);
			scopy(symlink, dest);
			strcat(dest, p);
		} else {
			dest = p;
		}
		goto top;
	}
	STPUTC('\0', p);
	p = grabstackstr(p);
	INTOFF;
	if (chdir(p) < 0) {
		INTON;
		return -1;
	}
	updatepwd(p);
	INTON;
	if (print && iflag)
		out1fmt("%s\n", p);
	return 0;
}
#endif /* SYMLINKS */



/*
 * Get the next component of the path name pointed to by cdcomppath.
 * This routine overwrites the string pointed to by cdcomppath.
 */

STATIC char *
getcomponent() {
	register char *p;
	char *start;

	if ((p = cdcomppath) == NULL)
		return NULL;
	start = cdcomppath;
	while (*p != '/' && *p != '\0')
		p++;
	if (*p == '\0') {
		cdcomppath = NULL;
	} else {
		*p++ = '\0';
		cdcomppath = p;
	}
	return start;
}



/*
 * Update curdir (the name of the current directory) in response to a
 * cd command.  We also call hashcd to let the routines in exec.c know
 * that the current directory has changed.
 */

void hashcd();

STATIC void
updatepwd(dir)
	char *dir;
	{
	char *new;
	char *p;

	hashcd();				/* update command hash table */
	cdcomppath = stalloc(strlen(dir) + 1);
	scopy(dir, cdcomppath);
	STARTSTACKSTR(new);
	if (*dir != '/') {
		if (curdir == NULL)
			return;
		p = curdir;
		while (*p)
			STPUTC(*p++, new);
		if (p[-1] == '/')
			STUNPUTC(new);
	}
	while ((p = getcomponent()) != NULL) {
		if (equal(p, "..")) {
			while (new > stackblock() && (STUNPUTC(new), *new) != '/');
		} else if (*p != '\0' && ! equal(p, ".")) {
			STPUTC('/', new);
			while (*p)
				STPUTC(*p++, new);
		}
	}
	if (new == stackblock())
		STPUTC('/', new);
	STACKSTRNUL(new);
	INTOFF;
	if (prevdir)
		ckfree(prevdir);
	prevdir = curdir;
	curdir = savestr(stackblock());
	INTON;
}



int
pwdcmd(argc, argv)
	int argc;
	char **argv; 
{
	getpwd();
	out1str(curdir);
	out1c('\n');
	return 0;
}



/*
 * Run /bin/pwd to find out what the current directory is.  We suppress
 * interrupts throughout most of this, but the user can still break out
 * of it by killing the pwd program.  If we already know the current
 * directory, this routine returns immediately.
 */

#define MAXPWD 256

STATIC void
getpwd() {
	char buf[MAXPWD];
	char *p;
	int i;
	int status;
	struct job *jp;
	int pip[2];

	if (curdir)
		return;
	INTOFF;
	if (pipe(pip) < 0)
		error("Pipe call failed");
	jp = makejob((union node *)NULL, 1);
	if (forkshell(jp, (union node *)NULL, FORK_NOJOB) == 0) {
		close(pip[0]);
		if (pip[1] != 1) {
			close(1);
			copyfd(pip[1], 1);
			close(pip[1]);
		}
		execl("/bin/pwd", "pwd", (char *)0);
		error("Cannot exec /bin/pwd");
	}
	close(pip[1]);
	pip[1] = -1;
	p = buf;
	while ((i = read(pip[0], p, buf + MAXPWD - p)) > 0
	     || (i == -1 && errno == EINTR)) {
		if (i > 0)
			p += i;
	}
	close(pip[0]);
	pip[0] = -1;
	status = waitforjob(jp);
	if (status != 0)
		error((char *)0);
	if (i < 0 || p == buf || p[-1] != '\n')
		error("pwd command failed");
	p[-1] = '\0';
	curdir = savestr(buf);
	INTON;
}
