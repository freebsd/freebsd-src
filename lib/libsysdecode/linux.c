/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Dmitry Chagin <dchagin@FreeBSD.orf>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/proc.h>
#include <stdbool.h>
#include <stdio.h>
#include <sysdecode.h>

#include "support.h"

#ifdef __aarch64__
#include <arm64/linux/linux.h>
#elif __i386__
#include <i386/linux/linux.h>
#elif __amd64__
#ifdef COMPAT_32BIT
#include <amd64/linux32/linux.h>
#else
#include <amd64/linux/linux.h>
#endif
#else
#error "Unsupported Linux arch"
#endif

#include <compat/linux/linux.h>
#include <compat/linux/linux_timer.h>

#define	X(a,b)	{ a, #b },
#define	XEND	{ 0, NULL }

#define	TABLE_START(n)	static struct name_table n[] = {
#define	TABLE_ENTRY	X
#define	TABLE_END	XEND };

#include "tables_linux.h"

#undef TABLE_START
#undef TABLE_ENTRY
#undef TABLE_END

void
sysdecode_linux_clockid(FILE *fp, clockid_t which)
{
	const char *str;
	clockid_t ci;
	pid_t pid;

	if (which >= 0) {
		str = lookup_value(clockids, which);
		if (str == NULL)
			fprintf(fp, "UNKNOWN(%d)", which);
		else
			fputs(str, fp);
		return;
	}
	if ((which & LINUX_CLOCKFD_MASK) == LINUX_CLOCKFD_MASK) {
		fputs("INVALID PERTHREAD|CLOCKFD", fp);
		goto pidp;
	}
	ci = LINUX_CPUCLOCK_WHICH(which);
	if (LINUX_CPUCLOCK_PERTHREAD(which) == true)
		fputs("THREAD|", fp);
	else
		fputs("PROCESS|", fp);
	str = lookup_value(clockcpuids, ci);
	if (str != NULL)
		fputs(str, fp);
	else {
		if (ci == LINUX_CLOCKFD)
			fputs("CLOCKFD", fp);
		else
			fprintf(fp, "UNKNOWN(%d)", which);
	}

pidp:
	pid = LINUX_CPUCLOCK_ID(which);
	fprintf(fp, "(%d)", pid);
}
