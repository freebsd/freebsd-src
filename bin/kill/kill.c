/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
/*
 * Important: This file is used both as a standalone program /bin/kill and
 * as a builtin for /bin/sh (#define SHELL).
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SHELL
#define main killcmd
#include "bltin/bltin.h"
#endif

static void nosig(const char *);
static void printsignals(FILE *);
static void usage(void) __dead2;

int
main(int argc, char *argv[])
{
	char signame[SIG2STR_MAX];
	long pidl;
	pid_t pid;
	int errors, numsig, ret;
	char *ep;

	if (argc < 2)
		usage();

	numsig = SIGTERM;

	argc--, argv++;
	if (!strcmp(*argv, "-l")) {
		argc--, argv++;
		if (argc > 1)
			usage();
		if (argc == 1) {
			if (!isdigit(**argv))
				usage();
			numsig = strtol(*argv, &ep, 10);
			if (!**argv || *ep)
				errx(2, "invalid signal number: %s", *argv);
			if (numsig >= 128)
				numsig -= 128;
			if (sig2str(numsig, signame) < 0)
				nosig(*argv);
			printf("%s\n", signame);
			return (0);
		}
		printsignals(stdout);
		return (0);
	}

	if (!strcmp(*argv, "-s")) {
		argc--, argv++;
		if (argc < 1) {
			warnx("option requires an argument -- s");
			usage();
		}
		if (strcmp(*argv, "0") == 0)
			numsig = 0;
		else if (str2sig(*argv, &numsig) < 0)
			nosig(*argv);
		argc--, argv++;
	} else if (**argv == '-' && *(*argv + 1) != '-') {
		++*argv;
		if (strcmp(*argv, "0") == 0)
			numsig = 0;
		else if (str2sig(*argv, &numsig) < 0)
			nosig(*argv);
		argc--, argv++;
	}

	if (argc > 0 && strncmp(*argv, "--", 2) == 0)
		argc--, argv++;

	if (argc == 0)
		usage();

	for (errors = 0; argc; argc--, argv++) {
#ifdef SHELL
		if (**argv == '%')
			ret = killjob(*argv, numsig);
		else
#endif
		{
			pidl = strtol(*argv, &ep, 10);
			/* Check for overflow of pid_t. */
			pid = (pid_t)pidl;
			if (!**argv || *ep || pid != pidl)
				errx(2, "illegal process id: %s", *argv);
			ret = kill(pid, numsig);
		}
		if (ret == -1) {
			warn("%s", *argv);
			errors = 1;
		}
	}

	return (errors);
}

static void
nosig(const char *name)
{

	warnx("unknown signal %s; valid signals:", name);
	printsignals(stderr);
#ifdef SHELL
	error(NULL);
#else
	exit(2);
#endif
}

static void
printsignals(FILE *fp)
{
	int n;

	for (n = 1; n < sys_nsig; n++) {
		(void)fprintf(fp, "%s", sys_signame[n]);
		if (n == (sys_nsig / 2) || n == (sys_nsig - 1))
			(void)fprintf(fp, "\n");
		else
			(void)fprintf(fp, " ");
	}
}

static void
usage(void)
{

	(void)fprintf(stderr, "%s\n%s\n%s\n%s\n",
		"usage: kill [-s signal_name] pid ...",
		"       kill -l [exit_status]",
		"       kill -signal_name pid ...",
		"       kill -signal_number pid ...");
#ifdef SHELL
	error(NULL);
#else
	exit(2);
#endif
}
