/*
 * Copyright (c) 1988, 1993, 1994
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
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)kill.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void nosig __P((char *));
void printsig __P((FILE *));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	const char *const *p;
	int errors, numsig, pid;
	char *ep;

	if (argc < 2)
		usage();

	if (!strcmp(*++argv, "-l")) {
		printsig(stdout);
		exit(0);
	}

	numsig = SIGTERM;
	if (**argv == '-') {
		++*argv;
		if (isalpha(**argv)) {
			if (!strncasecmp(*argv, "sig", 3))
				*argv += 3;
			for (numsig = NSIG, p = sys_signame + 1; --numsig; ++p)
				if (!strcasecmp(*p, *argv)) {
					numsig = p - sys_signame;
					break;
				}
			if (!numsig)
				nosig(*argv);
		} else if (isdigit(**argv)) {
			numsig = strtol(*argv, &ep, 10);
			if (!*argv || *ep)
				errx(1, "illegal signal number: %s", *argv);
			if (numsig <= 0 || numsig > NSIG)
				nosig(*argv);
		} else
			nosig(*argv);
		++argv;
	}

	if (!*argv)
		usage();

	for (errors = 0; *argv; ++argv) {
		pid = strtol(*argv, &ep, 10);
		if (!*argv || *ep) {
			warnx("illegal process id: %s", *argv);
			errors = 1;
		} else if (kill(pid, numsig) == -1) {
			warn("%s", *argv);
			errors = 1;
		}
	}
	exit(errors);
}

void
nosig(name)
	char *name;
{

	warnx("unknown signal %s; valid signals:", name);
	printsig(stderr);
	exit(1);
}

void
printsig(fp)
	FILE *fp;
{
	const char *const *p;
	int cnt;

	for (cnt = NSIG, p = sys_signame + 1; --cnt; ++p) {
		(void)fprintf(fp, "%s ", *p);
		if (cnt == NSIG / 2)
			(void)fprintf(fp, "\n");
	}
	(void)fprintf(fp, "\n");
}

void
usage()
{

	(void)fprintf(stderr, "usage: kill [-l] [-sig] pid ...\n");
	exit(1);
}
