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
 *
 *	$Id: cd.c,v 1.14 1997/02/22 13:58:22 peter Exp $
 */

#ifndef lint
static char const sccsid[] = "@(#)cd.c	8.2 (Berkeley) 5/4/95";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
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
#include "exec.h"
#include "redir.h"
#include "mystring.h"
#include "show.h"
#include "cd.h"

STATIC int docd __P((char *, int));
STATIC char *getcomponent __P((void));
STATIC void updatepwd __P((char *));

char *curdir = NULL;		/* current working directory */
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
	int print = 0;

	nextopt(nullstr);
	if ((dest = *argptr) == NULL && (dest = bltinlookup("HOME", 1)) == NULL)
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
 * Actually do the chdir.  In an interactive shell, print the
 * directory name if "print" is nonzero.
 */
STATIC int
docd(dest, print)
	char *dest;
	int print;
{

	TRACE(("docd(\"%s\", %d) called\n", dest, print));
	INTOFF;
	updatepwd(dest);
	if (chdir(stackblock()) < 0) {
		INTON;
		return -1;
	}
	hashcd();				/* update command hash table */
	if (prevdir)
		ckfree(prevdir);
	prevdir = curdir;
	curdir = savestr(stackblock());
	INTON;
	if (print && iflag)
		out1fmt("%s\n", stackblock());
	return 0;
}


/*
 * Get the next component of the path name pointed to by cdcomppath.
 * This routine overwrites the string pointed to by cdcomppath.
 */
STATIC char *
getcomponent()
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
 * Determine the new working directory, but don't actually enforce
 * any changes.
 */
STATIC void
updatepwd(dir)
	char *dir;
{
	char *new;
	char *p;

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
}


int
pwdcmd(argc, argv)
	int argc;
	char **argv;
{
	if (!getpwd())
		error("getcwd() failed: %s", strerror(errno));
	out1str(curdir);
	out1c('\n');
	return 0;
}


/*
 * Find out what the current directory is. If we already know the current
 * directory, this routine returns immediately.
 */
char *
getpwd()
{
	if (curdir)
		return (curdir);
	return ((curdir = getcwd(NULL, 0)));
}
