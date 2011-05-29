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
#include <libprocstat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "procstat.h"

void
procstat_cred(struct kinfo_proc *kipp)
{
	int i;
	int mib[4];
	int ngroups;
	size_t len;
	gid_t *groups = NULL;

	if (!hflag)
		printf("%5s %-16s %5s %5s %5s %5s %5s %5s %-20s\n", "PID",
		    "COMM", "EUID", "RUID", "SVUID", "EGID", "RGID", "SVGID",
		    "GROUPS");

	printf("%5d ", kipp->ki_pid);
	printf("%-16s ", kipp->ki_comm);
	printf("%5d ", kipp->ki_uid);
	printf("%5d ", kipp->ki_ruid);
	printf("%5d ", kipp->ki_svuid);
	printf("%5d ", kipp->ki_groups[0]);
	printf("%5d ", kipp->ki_rgid);
	printf("%5d ", kipp->ki_svgid);

	/*
	 * We may have too many groups to fit in kinfo_proc's statically
	 * sized storage.  If that occurs, attempt to retrieve them via
	 * sysctl.
	 */
	if (kipp->ki_cr_flags & KI_CRF_GRP_OVERFLOW) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_GROUPS;
		mib[3] = kipp->ki_pid;

		ngroups = sysconf(_SC_NGROUPS_MAX) + 1;
		len = ngroups * sizeof(gid_t);
		if((groups = malloc(len)) == NULL)
			err(-1, "malloc");

		if (sysctl(mib, 4, groups, &len, NULL, 0) == -1) {
			warn("sysctl: kern.proc.groups: %d "
			    "group list truncated", kipp->ki_pid);
			free(groups);
			groups = NULL;
		}
		ngroups = len / sizeof(gid_t);
	}
	if (groups == NULL) {
		ngroups = kipp->ki_ngroups;
		groups = kipp->ki_groups;
	}
	for (i = 0; i < ngroups; i++)
		printf("%s%d", (i > 0) ? "," : "", groups[i]);
	if (groups != kipp->ki_groups)
		free(groups);

	printf("\n");
}
