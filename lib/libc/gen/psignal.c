/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1993
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
 * Print the name of the signal indicated
 * along with the supplied message.
 */
#include "namespace.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

void
psignal(int sig, const char *s)
{
	const char *c;

	if (sig >= 0 && sig < NSIG)
		c = sys_siglist[sig];
	else
		c = "Unknown signal";
	if (s != NULL && *s != '\0') {
		(void)_write(STDERR_FILENO, s, strlen(s));
		(void)_write(STDERR_FILENO, ": ", 2);
	}
	(void)_write(STDERR_FILENO, c, strlen(c));
	(void)_write(STDERR_FILENO, "\n", 1);
}

void
psiginfo(const siginfo_t *si, const char *s)
{
	psignal(si->si_signo, s);
}

int
sig2str(int signum, char *str)
{
	char tmp[16];
	char *t, *p;
	int n;

	if (signum > 0 && signum < sys_nsig) {
		strcpy(str, sys_signame[signum]);
		return (0);
	}

	if (signum < SIGRTMIN || signum > SIGRTMAX)
		return (-1);

	if (signum <= (SIGRTMIN + SIGRTMAX) / 2) {
		strcpy(str, "RTMIN");
		n = signum - SIGRTMIN;
	} else {
		strcpy(str, "RTMAX");
		n = signum - SIGRTMAX;
	}

	if (n != 0) {
		/*
		 * This block does the equivalent of
		 * sprintf(str + 5, "%+d", n);
		 */
		if (n < 0) {
			str[5] = '-';
			n = -n;
		} else
			str[5] = '+';
		t = tmp;
		do {
			*t++ = "0123456789"[n % 10];
		} while (n /= 10);

		p = str + 6;
		do {
			*p++ = *--t;
		} while (t > tmp);
		*p = '\0';
	}

	return (0);
}

int
str2sig(const char * restrict str, int * restrict pnum)
{
	int n, sig;
	char *end;

	if (strncmp(str, "RTMIN", 5) == 0 || strncmp(str, "RTMAX", 5) == 0) {
		sig = (str[4] == 'X') ? SIGRTMAX : SIGRTMIN;
		n = 0;
		if (str[5] == '+' || str[5] == '-') {
			n = (int) strtol(str + 5, &end, 10);
			if (*end != '\0' || n == 0)
				return (-1);
		} else if (str[5] != '\0')
			return (-1);
		sig += n;
		if (sig < SIGRTMIN || sig > SIGRTMAX)
			return (-1);
		*pnum = sig;
		return (0);
	}

	if (str[0] >= '0' && str[0] <= '9') {
		sig = (int)strtol(str, &end, 10);
		if (*end == '\0' && sig > 0 && sig < sys_nsig) {
			*pnum = sig;
			return (0);
		}
	}

	for (sig = 1; sig < sys_nsig; sig++) {
		if (strcmp(sys_signame[sig], str) == 0) {
			*pnum = sig;
			return (0);
		}
	}

	return (-1);
}
