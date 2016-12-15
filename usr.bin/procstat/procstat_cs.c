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
#include <sys/cpuset.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <errno.h>
#include <libprocstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procstat.h"

void
procstat_cs(struct procstat *procstat, struct kinfo_proc *kipp)
{
	cpusetid_t cs;
	cpuset_t mask;
	struct kinfo_proc *kip;
	unsigned int count, i;
	int once, twice, lastcpu, cpu;

	if (!hflag)
		printf("%5s %6s %-19s %-19s %2s %4s %-7s\n", "PID",
		    "TID", "COMM", "TDNAME", "CPU", "CSID", "CPU MASK");

	kip = procstat_getprocs(procstat, KERN_PROC_PID | KERN_PROC_INC_THREAD,
	    kipp->ki_pid, &count);
	if (kip == NULL)
		return;
	kinfo_proc_sort(kip, count);
	for (i = 0; i < count; i++) {
		kipp = &kip[i];
		printf("%5d ", kipp->ki_pid);
		printf("%6d ", kipp->ki_tid);
		printf("%-19s ", strlen(kipp->ki_comm) ?
		    kipp->ki_comm : "-");
		printf("%-19s ", kinfo_proc_thread_name(kipp));
		if (kipp->ki_oncpu != 255)
			printf("%3d ", kipp->ki_oncpu);
		else if (kipp->ki_lastcpu != 255)
			printf("%3d ", kipp->ki_lastcpu);
		else
			printf("%3s ", "-");
		if (cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_TID,
		    kipp->ki_tid, &cs) != 0) {
			cs = CPUSET_INVALID;
		}
		printf("%4d ", cs);
		if ((cs != CPUSET_INVALID) && 
		    (cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID,
		    kipp->ki_tid, sizeof(mask), &mask) == 0)) {
			lastcpu = -1;
			once = 0;
			twice = 0;
			for (cpu = 0; cpu < CPU_SETSIZE; cpu++) {
				if (CPU_ISSET(cpu, &mask)) {
					if (once == 0) {
						printf("%d", cpu);
						once = 1;
					} else if (cpu == lastcpu + 1) {
						twice = 1;
					} else if (twice == 1) {
						printf("-%d,%d", lastcpu, cpu);
						twice = 0;
					} else
						printf(",%d", cpu);
					lastcpu = cpu;
				}
			}
			if (once && twice)
				printf("-%d", lastcpu);
		}
		printf("\n");
	}
	procstat_freeprocs(procstat, kip);
}
