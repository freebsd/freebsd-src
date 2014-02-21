/*-
 * Copyright (c) 2007 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <errno.h>
#include <libprocstat.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procstat.h"

static void
do_args(struct procstat *procstat, struct kinfo_proc *kipp, int env)
{
	int i;
	char **args;

	if (!hflag) {
		printf("%5s %-16s %-53s\n", "PID", "COMM",
		    env ? "ENVIRONMENT" : "ARGS");
	}

	args = env ? procstat_getenvv(procstat, kipp, 0) :
	    procstat_getargv(procstat, kipp, 0);

	printf("%5d %-16s", kipp->ki_pid, kipp->ki_comm);

	if (args == NULL) {
		printf(" -\n");
		return;
	}

	for (i = 0; args[i] != NULL; i++)
		printf(" %s", args[i]);
	printf("\n");
}

void
procstat_args(struct procstat *procstat, struct kinfo_proc *kipp)
{
	do_args(procstat, kipp, 0);
}

void
procstat_env(struct procstat *procstat, struct kinfo_proc *kipp)
{
	do_args(procstat, kipp, 1);
}
