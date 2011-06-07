/*-
 * Copyright (c) 2010 Konstantin Belousov
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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libprocstat.h>

#include "procstat.h"

static void
procstat_print_signame(int sig)
{
	char name[12];
	int i;

	if (!nflag && sig < sys_nsig) {
		strlcpy(name, sys_signame[sig], sizeof(name));
		for (i = 0; name[i] != 0; i++)
			name[i] = toupper(name[i]);
		printf("%-7s ", name);
	} else
		printf("%-7d ", sig);
}

static void
procstat_print_sig(const sigset_t *set, int sig, char flag)
{

	printf("%c", sigismember(set, sig) ? flag : '-');
}

void
procstat_sigs(struct procstat *prstat __unused, struct kinfo_proc *kipp)
{
	int j;
	pid_t pid;

	pid = kipp->ki_pid;
	if (!hflag)
		printf("%5s %-16s %-7s %4s\n", "PID", "COMM", "SIG", "FLAGS");

	for (j = 1; j <= _SIG_MAXSIG; j++) {
		printf("%5d ", pid);
		printf("%-16s ", kipp->ki_comm);
		procstat_print_signame(j);
		printf(" ");
		procstat_print_sig(&kipp->ki_siglist, j, 'P');
		procstat_print_sig(&kipp->ki_sigignore, j, 'I');
		procstat_print_sig(&kipp->ki_sigcatch, j, 'C');
		printf("\n");
	}
}

void
procstat_threads_sigs(struct procstat *prstat __unused, struct kinfo_proc *kipp)
{
	struct kinfo_proc *kip;
	pid_t pid;
	int error, name[4], j;
	unsigned int i;
	size_t len;

	pid = kipp->ki_pid;
	if (!hflag)
		printf("%5s %6s %-16s %-7s %4s\n", "PID", "TID", "COMM",
		     "SIG", "FLAGS");

	/*
	 * We need to re-query for thread information, so don't use *kipp.
	 */
	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_PID | KERN_PROC_INC_THREAD;
	name[3] = pid;

	len = 0;
	error = sysctl(name, 4, NULL, &len, NULL, 0);
	if (error < 0 && errno != ESRCH) {
		warn("sysctl: kern.proc.pid: %d", pid);
		return;
	}
	if (error < 0)
		return;

	kip = malloc(len);
	if (kip == NULL)
		err(-1, "malloc");

	if (sysctl(name, 4, kip, &len, NULL, 0) < 0) {
		warn("sysctl: kern.proc.pid: %d", pid);
		free(kip);
		return;
	}

	kinfo_proc_sort(kip, len / sizeof(*kipp));
	for (i = 0; i < len / sizeof(*kipp); i++) {
		kipp = &kip[i];
		for (j = 1; j <= _SIG_MAXSIG; j++) {
			printf("%5d ", pid);
			printf("%6d ", kipp->ki_tid);
			printf("%-16s ", kipp->ki_comm);
			procstat_print_signame(j);
			printf(" ");
			procstat_print_sig(&kipp->ki_siglist, j, 'P');
			procstat_print_sig(&kipp->ki_sigmask, j, 'B');
			printf("\n");
		}
	}
	free(kip);
}
