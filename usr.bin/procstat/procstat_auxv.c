/*-
 * Copyright (c) 2011 Mikolaj Golub
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

#include <machine/elf.h>

#include "procstat.h"

static Elf_Auxinfo auxv[256];

void
procstat_auxv(struct kinfo_proc *kipp)
{
	int error, name[4];
	size_t len, i;

	if (!hflag)
		printf("%5s %-16s %-53s\n", "PID", "COMM", "AUXV");

	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_AUXV;
	name[3] = kipp->ki_pid;
	len = sizeof(auxv) * sizeof(*auxv);
	error = sysctl(name, 4, auxv, &len, NULL, 0);
	if (error < 0 && errno != ESRCH) {
		warn("sysctl: kern.proc.auxv: %d: %d", kipp->ki_pid, errno);
		return;
	}
	if (error < 0)
		return;
	printf("%5d ", kipp->ki_pid);
	printf("%-16s", kipp->ki_comm);
	if (len == 0) {
		printf(" -\n");
		return;
	}
	for (i = 0; i < len; i++) {
		switch(auxv[i].a_type) {
		case AT_NULL:
			printf(" (%zu)\n", i + 1);
			return;
		case AT_IGNORE:
			printf(" AT_IGNORE=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_EXECFD:
			printf(" AT_EXECFD=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_PHDR:
			printf(" AT_PHDR=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_PHENT:
			printf(" AT_PHENT=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_PHNUM:
			printf(" AT_PHNUM=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_PAGESZ:
			printf(" AT_PAGESZ=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_BASE:
			printf(" AT_BASE=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_FLAGS:
			printf(" AT_FLAGS=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_ENTRY:
			printf(" AT_ENTRY=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
#ifdef AT_NOTELF
		case AT_NOTELF:
			printf(" AT_NOTELF=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
#endif
#ifdef AT_UID
		case AT_UID:
			printf(" AT_UID=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
#endif
#ifdef AT_EUID
		case AT_EUID:
			printf(" AT_EUID=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
#endif
#ifdef AT_GID
		case AT_GID:
			printf(" AT_GID=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
#endif
#ifdef AT_EGID
		case AT_EGID:
			printf(" AT_EGID=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
#endif
		case AT_EXECPATH:
			printf(" AT_EXECPATH=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_CANARY:
			printf(" AT_CANARY=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_CANARYLEN:
			printf(" AT_CANARYLEN=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_OSRELDATE:
			printf(" AT_OSRELDATE=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_NCPUS:
			printf(" AT_NCPUS=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_PAGESIZES:
			printf(" AT_PAGESIZES=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_PAGESIZESLEN:
			printf(" AT_PAGESIZESLEN=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_STACKPROT:
			printf(" AT_STACKPROT=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		case AT_COUNT:
			printf(" AT_COUNT=0x%lu",
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		default:
			printf(" %ld=0x%lu", (long)auxv[i].a_type,
			    (unsigned long)auxv[i].a_un.a_val);
			break;
		}
	}
	printf("\n");
}

