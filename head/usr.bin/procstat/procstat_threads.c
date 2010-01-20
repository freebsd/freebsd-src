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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procstat.h"

void
procstat_threads(pid_t pid, struct kinfo_proc *kipp)
{
	struct kinfo_proc *kip;
	int error, name[4];
	unsigned int i;
	const char *str;
	size_t len;

	if (!hflag)
		printf("%5s %6s %-16s %-16s %2s %4s %-7s %-9s\n", "PID",
		    "TID", "COMM", "TDNAME", "CPU", "PRI", "STATE", "WCHAN");

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
		printf("%5d ", pid);
		printf("%6d ", kipp->ki_tid);
		printf("%-16s ", strlen(kipp->ki_comm) ?
		    kipp->ki_comm : "-");
		printf("%-16s ", (strlen(kipp->ki_ocomm) &&
		    (strcmp(kipp->ki_comm, kipp->ki_ocomm) != 0)) ?
		    kipp->ki_ocomm : "-");
		if (kipp->ki_oncpu != 255)
			printf("%3d ", kipp->ki_oncpu);
		else if (kipp->ki_lastcpu != 255)
			printf("%3d ", kipp->ki_lastcpu);
		else
			printf("%3s ", "-");
		printf("%4d ", kipp->ki_pri.pri_level);
		switch (kipp->ki_stat) {
		case SRUN:
			str = "run";
			break;

		case SSTOP:
			str = "stop";
			break;

		case SSLEEP:
			str = "sleep";
			break;

		case SLOCK:
			str = "lock";
			break;

		case SWAIT:
			str = "wait";
			break;

		case SZOMB:
			str = "zomb";
			break;

		case SIDL:
			str = "idle";
			break;

		default:
			str = "??";
			break;
		}
		printf("%-7s ", str);
		if (kipp->ki_kiflag & KI_LOCKBLOCK) {
			printf("*%-8s ", strlen(kipp->ki_lockname) ?
			    kipp->ki_lockname : "-");
		} else {
			printf("%-9s ", strlen(kipp->ki_wmesg) ?
			    kipp->ki_wmesg : "-");
		}
		printf("\n");
	}
	free(kip);
}
