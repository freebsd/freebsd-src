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
 *
 * $xMach: xargs.c,v 1.6 2002/02/23 05:27:47 tim Exp $
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

static void	run(char **);
static void	usage(void);
void		strnsubst(char **, const char *, const char *, size_t);

static char echo[] = _PATH_ECHO;
static int pflag, tflag, rval, zflag;

extern char *environ[];

int
main(int argc, char **argv)
{
	long arg_max;
	int ch, cnt, count, Iflag, indouble, insingle, Jflag, jfound, Lflag;
	int nargs, nflag, nline, Rflag, wasquoted, foundeof, xflag;
	size_t linelen;
	const char *eofstr;
	char **av, **avj, **bxp, **ep, **exp, **xp;
	char *argp, *bbp, *ebp, *inpline, *p, *replstr;

	ep = environ;
	inpline = replstr = NULL;
	eofstr = "";
	cnt = count = Iflag = Jflag = jfound = Lflag = nflag = Rflag = xflag =
	    wasquoted = 0;

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
	while ((ch = getopt(argc, argv, "0E:I:J:L:n:pR:s:tx")) != -1)
		switch(ch) {
		case 'E':
			eofstr = optarg;
			break;
		case 'I':
			Iflag = 1;
			Lflag = 1;
			Rflag = 5;
			replstr = optarg;
			break;
		case 'J':
			Jflag = 1;
			replstr = optarg;
			break;
		case 'L':
			Lflag = atoi(optarg);
			break;
		case 'n':
			nflag = 1;
			if ((nargs = atoi(optarg)) <= 0)
				errx(1, "illegal argument count");
			break;
		case 'p':
			pflag = 1;
			break;
		case 'R':
			if (!Iflag)
				usage();
			if ((Rflag = atoi(optarg)) <= 0)
				errx(1, "illegal number of replacements");
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
	if (Iflag || Lflag)
		xflag = 1;
	if (replstr != NULL && *replstr == '\0')
		errx(1, "replstr may not be empty");

	/*
	 * Allocate pointers for the utility name, the utility arguments,
	 * the maximum arguments to be read from stdin and the trailing
	 * NULL.
	 */
	linelen = 1 + argc + nargs + 1;
	if ((av = bxp = malloc(linelen * sizeof(char **))) == NULL)
		err(1, "malloc");

	/*
	 * Use the user's name for the utility as argv[0], just like the
	 * shell.  Echo is the default.  Set up pointers for the user's
	 * arguments.
	 */
	if (!*argv)
		cnt = strlen((*bxp++ = echo));
	else {
		do {
			if (Jflag && strcmp(*argv, replstr) == 0) {
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

	if ((bbp = malloc((size_t)nline + 1)) == NULL)
		err(1, "malloc");
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
			count++;
			if (zflag)
				goto addch;

			/* Quotes do not escape newlines. */
arg1:			if (insingle || indouble)
				 errx(1, "unterminated quote");

arg2:
			foundeof = *eofstr != '\0' &&
			    strcmp(argp, eofstr) == 0;

			/* Do not make empty args unless they are quoted */
			if ((argp != p || wasquoted) && !foundeof) {
				*p++ = '\0';
				*xp++ = argp;
				if (Iflag) {
					size_t curlen;

					if (inpline == NULL)
						curlen = 0;
					else {
						/*
						 * If this string is not zero
						 * length, append a space for
						 * seperation before the next
						 * argument.
						 */
						if ((curlen = strlen(inpline)))
							strcat(inpline, " ");
					}
					curlen++;
					/*
					 * Allocate enough to hold what we will
					 * be holding in a secont, and to append
					 * a space next time through, if we have
					 * to.
					 */
					inpline = realloc(inpline, curlen + 2 +
					    strlen(argp));
					if (inpline == NULL)
						err(1, "realloc");
					if (curlen == 1)
						strcpy(inpline, argp);
					else
						strcat(inpline, argp);
				}
			}

			/*
			 * If max'd out on args or buffer, or reached EOF,
			 * run the command.  If xflag and max'd out on buffer
			 * but not on args, object.  Having reached the limit
			 * of input lines, as specified by -L is the same as
			 * maxing out on arguments.
			 */
			if (xp == exp || p > ebp || ch == EOF || (Lflag <= count && xflag) || foundeof) {
				if (xflag && xp != exp && p > ebp)
					errx(1, "insufficient space for arguments");
				if (jfound) {
					for (avj = argv; *avj; avj++)
						*xp++ = *avj;
				}
				if (Iflag) {
					char **tmp, **tmp2;
					size_t repls;
					int iter;

					/*
					 * Set up some locals, the number of
					 * times we may replace replstr with a
					 * line of input, a modifiable pointer
					 * to the head of the original argument
					 * list, and the number of iterations to
					 * perform -- the number of arguments.
					 */
					repls = Rflag;
					avj = av;
					iter = argc;

					/*
					 * Allocate memory to hold the argument
					 * list.
					 */
					tmp = malloc(linelen * sizeof(char **));
					if (tmp == NULL)
						err(1, "malloc");
					tmp2 = tmp;
					/*
					 * Just save the first argument, as it
					 * is the utility name, and we cannot
					 * be trusted to do strnsubst() to it.
					 */
					*tmp++ = strdup(*avj++);
					/*
					 * Now for every argument to utility,
					 * if we have not used up the number of
					 * replacements we are allowed to do, and
					 * if the argument contains at least one
					 * occurance of replstr, call strnsubst(),
					 * or else just save the string.
					 * Iterations over elements of avj and tmp
					 * are done where appropriate.
					 */
					while (--iter) {
						*tmp = *avj++;
						if (repls && strstr(*tmp, replstr) != NULL) {
							strnsubst(tmp++, replstr, inpline,
							    (size_t)255);
							repls--;
						} else {
							if ((*tmp = strdup(*tmp)) == NULL)
								err(1, "strdup");
							tmp++;
						}
					}
					/*
					 * NULL terminate the list of arguments,
					 * for run().
					 */
					*tmp = *xp = NULL;
					run(tmp2);
					/*
					 * From the tail to the head, free along
					 * the way.
					 */
					for (; tmp2 != tmp; tmp--)
						free(*tmp);
					/*
					 * Free the list.
					 */
					free(tmp2);
					/*
					 * Free the input line buffer, and create
					 * a new dummy.
					 */
					free(inpline);
					inpline = strdup("");
				} else {
					/*
					 * Mark the tail of the argument list with
					 * a NULL, and run() with it.
					 */
					*xp = NULL;
					run(av);
				}
				if (ch == EOF || foundeof)
					exit(rval);
				p = bbp;
				xp = bxp;
				count = 0;
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
			memcpy(bbp, argp, (size_t)cnt);
			p = (argp = bbp) + cnt;
			*p++ = ch;
			break;
		}
	/* NOTREACHED */
}

static void
run(char **argv)
{
	volatile int childerr;
	char **p;
	FILE *ttyfp;
	pid_t pid;
	int ch, status;

	if (tflag || pflag) {
		(void)fprintf(stderr, "%s", *argv);
		for (p = argv + 1; *p; ++p)
			(void)fprintf(stderr, " %s", *p);
		if (pflag && (ttyfp = fopen("/dev/tty", "r")) != NULL) {
			(void)fprintf(stderr, "?");
			(void)fflush(stderr);
			ch = getc(ttyfp);
			fclose(ttyfp);
			if (ch != 'y')
				return;
		} else {
			(void)fprintf(stderr, "\n");
			(void)fflush(stderr);
		}
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
	/* If we couldn't invoke the utility, exit. */
	if (childerr != 0)
		err(childerr == ENOENT ? 127 : 126, "%s", *argv);
	/* If utility signaled or exited with a value of 255, exit 1-125. */
	if (WIFSIGNALED(status) || WEXITSTATUS(status) == 255)
		exit(1);
	if (WEXITSTATUS(status))
		rval = 1;
}

static void
usage(void)
{
	fprintf(stderr,
"usage: xargs [-0pt] [-E eofstr] [-I replstr [-R replacements]] [-J replstr]\n"
"             [-L number] [-n number [-x] [-s size] [utility [argument ...]]\n");
	exit(1);
}
