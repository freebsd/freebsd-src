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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <stdio.h>

#include "procstat.h"

void
procstat_cred(pid_t pid, struct kinfo_proc *kipp)
{
	int i;

	if (!hflag)
		printf("%5s %5s %5s %5s %5s %5s %5s %-20s\n", "PID", "EUID",
		    "RUID", "SVUID", "EGID", "RGID", "SVGID", "GROUPS");

	printf("%5d ", pid);
	printf("%5d ", kipp->ki_uid);
	printf("%5d ", kipp->ki_ruid);
	printf("%5d ", kipp->ki_svuid);
	printf("%5d ", kipp->ki_groups[0]);
	printf("%5d ", kipp->ki_rgid);
	printf("%5d ", kipp->ki_svgid);
	for (i = 0; i < kipp->ki_ngroups; i++)
		printf("%s%d", (i > 0) ? "," : "", kipp->ki_groups[i]);
	printf("\n");
}
