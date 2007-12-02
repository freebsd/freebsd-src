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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "procstat.h"

void
procstat_vm(pid_t pid, struct kinfo_proc *kipp)
{
	struct kinfo_vmentry *freep, *kve;
	int error, i, name[4], ptrwidth;
	const char *str;
	size_t len;

	ptrwidth = 2*sizeof(void *) + 2;
	if (!hflag)
		printf("%5s %*s %*s %3s %4s %4s %3s %3s %2s %-2s %-s\n",
		    "PID", ptrwidth, "START", ptrwidth, "END", "PRT", "RES",
		    "PRES", "REF", "SHD", "FL", "TP", "PATH");

	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_VMMAP;
	name[3] = pid;

	len = 0;
	error = sysctl(name, 4, NULL, &len, NULL, 0);
	if (error < 0 && errno != ESRCH && errno != EPERM) {
		warn("sysctl: kern.proc.vmmap: %d", pid);
		return;
	}
	if (error < 0)
		return;

	/*
	 * Especially if running procstat -sv, we may need room for more
	 * mappings when printing than were present when we queried, so pad
	 * out the allocation a bit.
	 */
	len += sizeof(*kve) * 3;
	freep = kve = malloc(len);
	if (kve == NULL)
		err(-1, "malloc");
	if (sysctl(name, 4, kve, &len, NULL, 0) < 0) {
		warn("sysctl: kern.proc.vmmap: %d", pid);
		free(freep);
		return;
	}

	for (i = 0; i < (len / sizeof(*kve)); i++, kve++) {
		if (kve->kve_structsize != sizeof(*kve))
			errx(-1, "kinfo_vmentry structure mismatch");
		printf("%5d ", pid);
		printf("%*p ", ptrwidth, kve->kve_start);
		printf("%*p ", ptrwidth, kve->kve_end);
		printf("%s", kve->kve_protection & KVME_PROT_READ ? "r" : "-");
		printf("%s", kve->kve_protection & KVME_PROT_WRITE ? "w" : "-");
		printf("%s ", kve->kve_protection & KVME_PROT_EXEC ? "x" : "-");
		printf("%4d ", kve->kve_resident);
		printf("%4d ", kve->kve_private_resident);
		printf("%3d ", kve->kve_ref_count);
		printf("%3d ", kve->kve_shadow_count);
		printf("%-1s", kve->kve_flags & KVME_FLAG_COW ? "C" : "-");
		printf("%-1s ", kve->kve_flags & KVME_FLAG_NEEDS_COPY ? "N" :
		    "-");
		switch (kve->kve_type) {
		case KVME_TYPE_NONE:
			str = "--";
			break;
		case KVME_TYPE_DEFAULT:
			str = "df";
			break;
		case KVME_TYPE_VNODE:
			str = "vn";
			break;
		case KVME_TYPE_SWAP:
			str = "sw";
			break;
		case KVME_TYPE_DEVICE:
			str = "dv";
			break;
		case KVME_TYPE_PHYS:
			str = "ph";
			break;
		case KVME_TYPE_DEAD:
			str = "dd";
			break;
		case KVME_TYPE_UNKNOWN:
		default:
			str = "??";
			break;
		}
		printf("%-2s ", str);
		printf("%-s\n", kve->kve_path);
	}
	free(freep);
}
