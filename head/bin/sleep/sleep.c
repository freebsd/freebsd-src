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

#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)sleep.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

void usage(void);

int
main(int argc, char *argv[])
{
	struct timespec time_to_sleep;
	long l;
	int neg;
	char *p;

	if (argc != 2) {
		usage();
		return(1);
	}

	p = argv[1];

	/* Skip over leading whitespaces. */
	while (isspace((unsigned char)*p))
		++p;

	/* Check for optional `+' or `-' sign. */
	neg = 0;
	if (*p == '-') {
		neg = 1;
		++p;
		if (!isdigit((unsigned char)*p) && *p != '.') {
			usage();
			return(1);
		}
	}
	else if (*p == '+')
		++p;

	/* Calculate seconds. */
	if (isdigit((unsigned char)*p)) {
		l = strtol(p, &p, 10);
		if (l > INT_MAX) {
			/*
			 * Avoid overflow when `seconds' is huge.  This assumes
			 * that the maximum value for a time_t is <= INT_MAX.
			 */
			l = INT_MAX;
		}
	} else
		l = 0;
	time_to_sleep.tv_sec = (time_t)l;

	/* Calculate nanoseconds. */
	time_to_sleep.tv_nsec = 0;

	if (*p == '.') {		/* Decimal point. */
		l = 100000000L;
		do {
			if (isdigit((unsigned char)*++p))
				time_to_sleep.tv_nsec += (*p - '0') * l;
			else
				break;
			l /= 10;
		} while (l);
	}

	if (!neg && (time_to_sleep.tv_sec > 0 || time_to_sleep.tv_nsec > 0))
		(void)nanosleep(&time_to_sleep, (struct timespec *)NULL);

	return(0);
}

void
usage(void)
{
	const char msg[] = "usage: sleep seconds\n";

	write(STDERR_FILENO, msg, sizeof(msg) - 1);
}
