/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John B. Roll Jr.
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
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)xargs.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

int tflag, rval;
int zflag;

void run __P((char **));
static void usage __P((void));

static char echo[] = _PATH_ECHO;

int
main(argc, argv, env)
	int argc;
	char **argv, **env;
{
	int ch;
	char *p, *bbp, **bxp, *ebp, **exp, **xp;
	int cnt, jfound, indouble, insingle;
	int nargs, nflag, nline, xflag, wasquoted;
	char **av, **avj, *argp, **ep, *replstr;
	long arg_max;

	ep = env;
	jfound = 0;
	replstr = NULL;			/* set if user requests -J */

	/*
	 * POSIX.2 limits the exec line length to ARG_MAX - 2K.  Running that
	 * caused some E2BIG errors, so it was changed to ARG_MAX - 4K.  Given
	 * that the smallest argument is 2 bytes in length, this means that
	 * the number of arguments is limited to:
	 *
	 *	 (ARG_MAX - 4K - LENGTH(utility + arguments)) / 2.
	 *
	 * We arbitrarily limit the number of arguments to 5000.  This is
	 * allowed by POSIX.2 as long as the resulting minimum exec line is
	 * at least LINE_MAX.  Realloc'ing as necessary is possible, but
	 * probably not worthwhile.
	 */
	nargs = 5000;
	if ((arg_max = sysconf(_SC_ARG_MAX)) == -1)
		errx(1, "sysconf(_SC_ARG_MAX) failed");
	nline = arg_max - 4 * 1024;
	while (*ep) {
		/* 1 byte for each '\0' */
		nline -= strlen(*ep++) + 1 + sizeof(*ep);
	}
	nflag = xflag = wasquoted = 0;
	while ((ch = getopt(argc, argv, "0J:n:s:tx")) != -1)
		switch(ch) {
		case 'J':
			replstr = optarg;
			break;
		case 'n':
			nflag = 1;
			if ((nargs = atoi(optarg)) <= 0)
				errx(1, "illegal argument count");
			break;
		case 's':
			nline = atoi(optarg);
			break;
		case 't':
			tflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		case '0':
			zflag = 1;
			break;
		case '?':
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (xflag && !nflag)
		usage();

	/*
	 * Allocate pointers for the utility name, the utility arguments,
	 * the maximum arguments to be read from stdin and the trailing
	 * NULL.
	 */
	if (!(av = bxp =
	    malloc((u_int)(1 + argc + nargs + 1) * sizeof(char **))))
		errx(1, "malloc");

	/*
	 * Use the user's name for the utility as argv[0], just like the
	 * shell.  Echo is the default.  Set up pointers for the user's
	 * arguments.
	 */
	if (!*argv)
		cnt = strlen((*bxp++ = echo));
	else {
		cnt = 0;
		do {
			if (replstr && strcmp(*argv, replstr) == 0) {
				jfound = 1;
				argv++;
				for (avj = argv; *avj; avj++)
					cnt += strlen(*avj) + 1;
				break;
			}
			cnt += strlen(*bxp++ = *argv) + 1;
		} while (*++argv);
	}

	/*
	 * Set up begin/end/traversing pointers into the array.  The -n
	 * count doesn't include the trailing NULL pointer, so the malloc
	 * added in an extra slot.
	 */
	exp = (xp = bxp) + nargs;

	/*
	 * Allocate buffer space for the arguments read from stdin and the
	 * trailing NULL.  Buffer space is defined as the default or specified
	 * space, minus the length of the utility name and arguments.  Set up
	 * begin/end/traversing pointers into the array.  The -s count does
	 * include the trailing NULL, so the malloc didn't add in an extra
	 * slot.
	 */
	nline -= cnt;
	if (nline <= 0)
		errx(1, "insufficient space for command");

	if (!(bbp = malloc((u_int)nline + 1)))
		errx(1, "malloc");
	ebp = (argp = p = bbp) + nline - 1;

	for (insingle = indouble = 0;;)
		switch(ch = getchar()) {
		case EOF:
			/* No arguments since last exec. */
			if (p == bbp)
				exit(rval);
			goto arg1;
		case ' ':
		case '\t':
			/* Quotes escape tabs and spaces. */
			if (insingle || indouble || zflag)
				goto addch;
			goto arg2;
		case '\0':
			if (zflag)
				goto arg2;
			goto addch;
		case '\n':
			if (zflag)
				goto addch;

			/* Quotes do not escape newlines. */
arg1:			if (insingle || indouble)
				 errx(1, "unterminated quote");

arg2:
			/* Do not make empty args unless they are quoted */
			if (argp != p || wasquoted) {
				*p++ = '\0';
				*xp++ = argp;
			}

			/*
			 * If max'd out on args or buffer, or reached EOF,
			 * run the command.  If xflag and max'd out on buffer
			 * but not on args, object.
			 */
			if (xp == exp || p > ebp || ch == EOF) {
				if (xflag && xp != exp && p > ebp)
					errx(1, "insufficient space for arguments");
				if (jfound) {
					for (avj = argv; *avj; avj++)
						*xp++ = *avj;
				}
				*xp = NULL;
				run(av);
				if (ch == EOF)
					exit(rval);
				p = bbp;
				xp = bxp;
			}
			argp = p;
			wasquoted = 0;
			break;
		case '\'':
			if (indouble || zflag)
				goto addch;
			insingle = !insingle;
			wasquoted = 1;
			break;
		case '"':
			if (insingle || zflag)
				goto addch;
			indouble = !indouble;
			wasquoted = 1;
			break;
		case '\\':
			if (zflag)
				goto addch;
			/* Backslash escapes anything, is escaped by quotes. */
			if (!insingle && !indouble && (ch = getchar()) == EOF)
				errx(1, "backslash at EOF");
			/* FALLTHROUGH */
		default:
addch:			if (p < ebp) {
				*p++ = ch;
				break;
			}

			/* If only one argument, not enough buffer space. */
			if (bxp == xp)
				errx(1, "insufficient space for argument");
			/* Didn't hit argument limit, so if xflag object. */
			if (xflag)
				errx(1, "insufficient space for arguments");

			if (jfound) {
				for (avj = argv; *avj; avj++)
					*xp++ = *avj;
			}
			*xp = NULL;
			run(av);
			xp = bxp;
			cnt = ebp - argp;
			bcopy(argp, bbp, cnt);
			p = (argp = bbp) + cnt;
			*p++ = ch;
			break;
		}
	/* NOTREACHED */
}

void
run(argv)
	char **argv;
{
	volatile int childerr;
	char **p;
	pid_t pid;
	int status;

	if (tflag) {
		(void)fprintf(stderr, "%s", *argv);
		for (p = argv + 1; *p; ++p)
			(void)fprintf(stderr, " %s", *p);
		(void)fprintf(stderr, "\n");
		(void)fflush(stderr);
	}
	childerr = 0;
	switch(pid = vfork()) {
	case -1:
		err(1, "vfork");
	case 0:
		execvp(argv[0], argv);
		childerr = errno;
		_exit(1);
	}
	pid = waitpid(pid, &status, 0);
	if (pid == -1)
		err(1, "waitpid");
	/* If we couldn't invoke the utility, exit 127. */
	if (childerr != 0) {
		errno = childerr;
		warn("%s", argv[0]);
		exit(127);
	}
	/* If utility signaled or exited with a value of 255, exit 1-125. */
	if (WIFSIGNALED(status) || WEXITSTATUS(status) == 255)
		exit(1);
	if (WEXITSTATUS(status))
		rval = 1;
}

static void
usage()
{
	fprintf(stderr,
	    "usage: xargs [-0t] [-J replstr] [-n number [-x]] [-s size]\n"
	    "           [utility [argument ...]]\n");
	exit(1);
}
