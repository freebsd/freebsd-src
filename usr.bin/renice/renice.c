/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1989, 1993
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)renice.c	8.1 (Berkeley) 6/9/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int	donice(int, int, int, bool);
static int	getnum(const char *, const char *, int *);
static void	usage(void);

/*
 * Change the priority (nice) of processes
 * or groups of processes which are already
 * running.
 */
int
main(int argc, char *argv[])
{
	struct passwd *pwd;
	bool havedelim = false, haveprio = false, incr = false;
	int errs = 0, prio = 0, who = 0, which = PRIO_PROCESS;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (!havedelim) {
			/* can occur at any time prior to delimiter */
			if (strcmp(*argv, "-g") == 0) {
				which = PRIO_PGRP;
				continue;
			}
			if (strcmp(*argv, "-u") == 0) {
				which = PRIO_USER;
				continue;
			}
			if (strcmp(*argv, "-p") == 0) {
				which = PRIO_PROCESS;
				continue;
			}
			if (strcmp(*argv, "--") == 0) {
				havedelim = true;
				continue;
			}
			if (strcmp(*argv, "-n") == 0) {
				/* may occur only once, prior to priority */
				if (haveprio || incr || argc < 2)
					usage();
				incr = true;
				(void)argc--, argv++;
				/* fall through to priority */
			}
		}
		if (!haveprio) {
			/* must occur exactly once, prior to target */
			if (getnum("priority", *argv, &prio))
				return (1);
			haveprio = true;
			continue;
		}
		if (which == PRIO_USER) {
			if ((pwd = getpwnam(*argv)) != NULL)
				who = pwd->pw_uid;
			else if (getnum("uid", *argv, &who)) {
				errs++;
				continue;
			} else if (who < 0) {
				warnx("%s: bad value", *argv);
				errs++;
				continue;
			}
		} else {
			if (getnum("pid", *argv, &who)) {
				errs++;
				continue;
			}
			if (who < 0) {
				warnx("%s: bad value", *argv);
				errs++;
				continue;
			}
		}
		errs += donice(which, who, prio, incr);
	}
	if (!haveprio)
		usage();
	exit(errs != 0);
}

static int
donice(int which, int who, int prio, bool incr)
{
	int oldprio;

	errno = 0;
	oldprio = getpriority(which, who);
	if (oldprio == -1 && errno) {
		warn("%d: getpriority", who);
		return (1);
	}
	if (incr)
		prio = oldprio + prio;
	if (prio > PRIO_MAX)
		prio = PRIO_MAX;
	if (prio < PRIO_MIN)
		prio = PRIO_MIN;
	if (setpriority(which, who, prio) < 0) {
		warn("%d: setpriority", who);
		return (1);
	}
	fprintf(stderr, "%d: old priority %d, new priority %d\n", who,
	    oldprio, prio);
	return (0);
}

static int
getnum(const char *com, const char *str, int *val)
{
	long v;
	char *ep;

	errno = 0;
	v = strtol(str, &ep, 10);
	if (v < INT_MIN || v > INT_MAX || errno == ERANGE) {
		warnx("%s argument %s is out of range.", com, str);
		return (1);
	}
	if (ep == str || *ep != '\0' || errno != 0) {
		warnx("%s argument %s is invalid.", com, str);
		return (1);
	}

	*val = (int)v;
	return (0);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n",
"usage: renice priority [[-p] pid ...] [[-g] pgrp ...] [[-u] user ...]",
"       renice -n increment [[-p] pid ...] [[-g] pgrp ...] [[-u] user ...]");
	exit(1);
}
