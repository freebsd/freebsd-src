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
static char sccsid[] = "@(#)cd.c	8.2 (Berkeley) 5/4/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

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
#include "exec.h"
#include "redir.h"
#include "mystring.h"
#include "show.h"
#include "cd.h"

STATIC int cdlogical(char *);
STATIC int cdphysical(char *);
STATIC int docd(char *, int, int);
STATIC char *getcomponent(void);
STATIC int updatepwd(char *);

STATIC char *curdir = NULL;	/* current working directory */
STATIC char *prevdir;		/* previous working directory */
STATIC char *cdcomppath;

int
cdcmd(int argc, char **argv)
{
	char *dest;
	char *path;
	char *p;
	struct stat statb;
	int ch, phys, print = 0;

	optreset = 1; optind = 1; opterr = 0; /* initialize getopt */
	phys = Pflag;
	while ((ch = getopt(argc, argv, "LP")) != -1) {
		switch (ch) {
		case 'L':
			phys = 0;
			break;
		case 'P':
			phys = 1;
			break;
		default:
			error("unknown option: -%c", optopt);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		error("too many arguments");

	if ((dest = *argv) == NULL && (dest = bltinlookup("HOME", 1)) == NULL)
		error("HOME not set");
	if (*dest == '\0')
		dest = ".";
	if (dest[0] == '-' && dest[1] == '\0') {
		dest = prevdir ? prevdir : curdir;
		if (dest)
			print = 1;
		else
			dest = ".";
	}
	if (*dest == '/' || (path = bltinlookup("CDPATH", 1)) == NULL)
		path = nullstr;
	while ((p = padvance(&path, dest)) != NULL) {
		if (stat(p, &statb) >= 0 && S_ISDIR(statb.st_mode)) {
			if (!print) {
				/*
				 * XXX - rethink
				 */
				if (p[0] == '.' && p[1] == '/' && p[2] != '\0')
					print = strcmp(p + 2, dest);
				else
					print = strcmp(p, dest);
			}
			if (docd(p, print, phys) >= 0)
				return 0;
		}
	}
	error("can't cd to %s", dest);
	/*NOTREACHED*/
	return 0;
}


/*
 * Actually change the directory.  In an interactive shell, print the
 * directory name if "print" is nonzero.
 */
STATIC int
docd(char *dest, int print, int phys)
{

	TRACE(("docd(\"%s\", %d, %d) called\n", dest, print, phys));

	/* If logical cd fails, fall back to physical. */
	if ((phys || cdlogical(dest) < 0) && cdphysical(dest) < 0)
		return (-1);

	if (print && iflag && curdir)
		out1fmt("%s\n", curdir);

	return 0;
}

STATIC int
cdlogical(char *dest)
{
	char *p;
	char *q;
	char *component;
	struct stat statb;
	int first;
	int badstat;

	/*
	 *  Check each component of the path. If we find a symlink or
	 *  something we can't stat, clear curdir to force a getcwd()
	 *  next time we get the value of the current directory.
	 */
	badstat = 0;
	cdcomppath = stalloc(strlen(dest) + 1);
	scopy(dest, cdcomppath);
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
		if (lstat(stackblock(), &statb) < 0) {
			badstat = 1;
			break;
		}
	}

	INTOFF;
	if (updatepwd(badstat ? NULL : dest) < 0 || chdir(curdir) < 0) {
		INTON;
		return (-1);
	}
	INTON;
	return (0);
}

STATIC int
cdphysical(char *dest)
{

	INTOFF;
	if (chdir(dest) < 0 || updatepwd(NULL) < 0) {
		INTON;
		return (-1);
	}
	INTON;
	return (0);
}

/*
 * Get the next component of the path name pointed to by cdcomppath.
 * This routine overwrites the string pointed to by cdcomppath.
 */
STATIC char *
getcomponent(void)
{
	char *p;
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
STATIC int
updatepwd(char *dir)
{
	char *new;
	char *p;

	hashcd();				/* update command hash table */

	/*
	 * If our argument is NULL, we don't know the current directory
	 * any more because we traversed a symbolic link or something
	 * we couldn't stat().
	 */
	if (dir == NULL || curdir == NULL)  {
		if (prevdir)
			ckfree(prevdir);
		INTOFF;
		prevdir = curdir;
		curdir = NULL;
		if (getpwd() == NULL) {
			INTON;
			return (-1);
		}
		setvar("PWD", curdir, VEXPORT);
		setvar("OLDPWD", prevdir, VEXPORT);
		INTON;
		return (0);
	}
	cdcomppath = stalloc(strlen(dir) + 1);
	scopy(dir, cdcomppath);
	STARTSTACKSTR(new);
	if (*dir != '/') {
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
	setvar("PWD", curdir, VEXPORT);
	setvar("OLDPWD", prevdir, VEXPORT);
	INTON;

	return (0);
}

int
pwdcmd(int argc, char **argv)
{
	char buf[PATH_MAX];
	int ch, phys;

	optreset = 1; optind = 1; opterr = 0; /* initialize getopt */
	phys = Pflag;
	while ((ch = getopt(argc, argv, "LP")) != -1) {
		switch (ch) {
		case 'L':
			phys = 0;
			break;
		case 'P':
			phys = 1;
			break;
		default:
			error("unknown option: -%c", optopt);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		error("too many arguments");

	if (!phys && getpwd()) {
		out1str(curdir);
		out1c('\n');
	} else {
		if (getcwd(buf, sizeof(buf)) == NULL)
			error(".: %s", strerror(errno));
		out1str(buf);
		out1c('\n');
	}

	return 0;
}

/*
 * Find out what the current directory is. If we already know the current
 * directory, this routine returns immediately.
 */
char *
getpwd(void)
{
	char buf[PATH_MAX];

	if (curdir)
		return curdir;
	if (getcwd(buf, sizeof(buf)) == NULL) {
		char *pwd = getenv("PWD");
		struct stat stdot, stpwd;

		if (pwd && *pwd == '/' && stat(".", &stdot) != -1 &&
		    stat(pwd, &stpwd) != -1 &&
		    stdot.st_dev == stpwd.st_dev &&
		    stdot.st_ino == stpwd.st_ino) {
			curdir = savestr(pwd);
			return curdir;
		}
		return NULL;
	}
	curdir = savestr(buf);

	return curdir;
}
