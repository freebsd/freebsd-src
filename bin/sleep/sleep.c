/*-
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

#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void usage(void) __dead2;

static volatile sig_atomic_t report_requested;
static void
report_request(int signo __unused)
{

	report_requested = 1;
}

int
main(int argc, char *argv[])
{
	struct timespec time_to_sleep;
	double d, seconds;
	time_t original;
	char unit;
	char buf[2];
	int i, matches;

	if (caph_limit_stdio() < 0 || caph_enter() < 0)
		err(1, "capsicum");

	if (argc < 2)
		usage();

	seconds = 0;
	for (i = 1; i < argc; i++) {
		matches = sscanf(argv[i], "%lf%c%1s", &d, &unit, buf);
		if (matches == 2)
			switch(unit) {
			case 'd':
				d *= 24;
				/* FALLTHROUGH */
			case 'h':
				d *= 60;
				/* FALLTHROUGH */
			case 'm':
				d *= 60;
				/* FALLTHROUGH */
			case 's':
				break;
			default:
				usage();
			}
		else
			if (matches != 1)
				usage();
		seconds += d;
	}
	if (seconds > INT_MAX)
		usage();
	if (seconds <= 0)
		return (0);
	original = time_to_sleep.tv_sec = (time_t)seconds;
	time_to_sleep.tv_nsec = 1e9 * (seconds - time_to_sleep.tv_sec);

	signal(SIGINFO, report_request);

	/*
	 * Note: [EINTR] is supposed to happen only when a signal was handled
	 * but the kernel also returns it when a ptrace-based debugger
	 * attaches. This is a bug but it is hard to fix.
	 */
	while (nanosleep(&time_to_sleep, &time_to_sleep) != 0) {
		if (report_requested) {
			/* Reporting does not bother with nanoseconds. */
			warnx("about %d second(s) left out of the original %d",
			    (int)time_to_sleep.tv_sec, (int)original);
			report_requested = 0;
		} else if (errno != EINTR)
			err(1, "nanosleep");
	}
	return (0);
}

static void
usage(void)
{

	fprintf(stderr, "usage: sleep number[unit] ...\n");
	fprintf(stderr, "Unit can be 's' (seconds, the default), "
			"m (minutes), h (hours), or d (days).\n");
	exit(1);
}
