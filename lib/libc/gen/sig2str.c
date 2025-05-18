/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Ricardo Branco <rbranco@suse.de>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Translate between signal names and numbers
 */
#include "namespace.h"
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ssp/ssp.h>
#include "un-namespace.h"

static const char rtmin_str[] = "RTMIN";
static const char rtmax_str[] = "RTMAX";

int
__ssp_real(sig2str)(int signum, char *str)
{
	if (signum <= 0 || signum > SIGRTMAX)
		return (-1);

	if (signum < sys_nsig)
		(void)strlcpy(str, sys_signame[signum], SIG2STR_MAX);
	else if (signum < SIGRTMIN)
		(void)snprintf(str, SIG2STR_MAX, "%d", signum);
	else if (signum == SIGRTMIN)
		(void)strlcpy(str, rtmin_str, SIG2STR_MAX);
	else if (signum == SIGRTMAX)
		(void)strlcpy(str, rtmax_str, SIG2STR_MAX);
	else if (signum <= (SIGRTMIN + SIGRTMAX) / 2)
		(void)snprintf(str, SIG2STR_MAX, "%s+%d",
		    rtmin_str, signum - SIGRTMIN);
	else
		(void)snprintf(str, SIG2STR_MAX, "%s-%d",
		    rtmax_str, SIGRTMAX - signum);

	return (0);
}

int
str2sig(const char * restrict str, int * restrict pnum)
{
	const char *errstr;
	long long n;
	int sig;
	int rtend = sizeof(rtmin_str) - 1;

	if (strncasecmp(str, "SIG", 3) == 0)
		str += 3;

	if (strncasecmp(str, rtmin_str, sizeof(rtmin_str) - 1) == 0 ||
	    strncasecmp(str, rtmax_str, sizeof(rtmax_str) - 1) == 0) {
		sig = (toupper(str[4]) == 'X') ? SIGRTMAX : SIGRTMIN;
		n = 0;
		if (str[rtend] == '+' || str[rtend] == '-') {
			n = strtonum(str + rtend, INT_MIN, INT_MAX, &errstr);
			if (n == 0 || errstr != NULL)
				return (-1);
		} else if (str[rtend] != '\0') {
			return (-1);
		}
		sig += (int)n;
		if (sig < SIGRTMIN || sig > SIGRTMAX)
			return (-1);
		*pnum = sig;
		return (0);
	}

	if (isdigit((unsigned char)str[0])) {
		n = strtonum(str, 1, SIGRTMAX, &errstr);
		if (errstr == NULL) {
			*pnum = (int)n;
			return (0);
		}
	}

	for (sig = 1; sig < sys_nsig; sig++) {
		if (strcasecmp(sys_signame[sig], str) == 0) {
			*pnum = sig;
			return (0);
		}
	}

	return (-1);
}
