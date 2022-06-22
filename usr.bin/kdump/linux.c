/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Dmitry Chagin <dchagin@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/ktrace.h>
#include <stddef.h>
#include <sysdecode.h>

#include "kdump.h"

#ifdef __amd64__
#include <amd64/linux/linux_syscall.h>
#include <amd64/linux32/linux32_syscall.h>
#elif __aarch64__
#include <arm64/linux/linux_syscall.h>
#elif __i386__
#include <i386/linux/linux_syscall.h>
#endif

static void
print_linux_signal(int signo)
{
	const char *signame;

	signame = sysdecode_linux_signal(signo);
	if (signame != NULL)
		printf("%s", signame);
	else
		printf("SIG %d", signo);
}

void
ktrsyscall_linux(struct ktr_syscall *ktr, register_t **resip,
    int *resnarg, char *resc)
{
	int narg = ktr->ktr_narg;
	register_t *ip, *first;
	int quad_align, quad_slots;
	char c;

	ip = first = &ktr->ktr_args[0];
	c = *resc;
	quad_align = 0;
	quad_slots = 1;
	switch (ktr->ktr_code) {
	case LINUX_SYS_linux_clock_gettime:
	case LINUX_SYS_linux_clock_settime:
	case LINUX_SYS_linux_clock_getres:
	case LINUX_SYS_linux_timer_create:
		putchar('(');
		sysdecode_linux_clockid(stdout, *ip);
		c = ',';
		ip++;
		narg--;
		break;
	case LINUX_SYS_linux_kill:
	case LINUX_SYS_linux_tkill:
	case LINUX_SYS_linux_rt_sigqueueinfo:
		print_number(ip, narg, c);
		putchar(',');
		print_linux_signal(*ip);
		ip++;
		narg--;
		break;
	case LINUX_SYS_linux_tgkill:
	case LINUX_SYS_linux_rt_tgsigqueueinfo:
		print_number(ip, narg, c);
		print_number(ip, narg, c);
		putchar(',');
		print_linux_signal(*ip);
		ip++;
		narg--;
		break;
	case LINUX_SYS_linux_rt_sigaction:
		putchar('(');
		print_linux_signal(*ip);
		ip++;
		narg--;
		c = ',';
		break;
	case LINUX_SYS_linux_ftruncate:
	case LINUX_SYS_linux_truncate:
		print_number(ip, narg, c);
		print_number64(first, ip, narg, c);
		break;
	}
	*resc = c;
	*resip = ip;
	*resnarg = narg;
}

#if defined(__amd64__)
void
ktrsyscall_linux32(struct ktr_syscall *ktr, register_t **resip,
    int *resnarg, char *resc)
{
	int narg = ktr->ktr_narg;
	register_t *ip, *first;
	int quad_align, quad_slots;
	char c;

	ip = first = &ktr->ktr_args[0];
	c = *resc;
	quad_align = 0;
	quad_slots = 2;
	switch (ktr->ktr_code) {
	case LINUX32_SYS_linux_clock_gettime:
	case LINUX32_SYS_linux_clock_settime:
	case LINUX32_SYS_linux_clock_getres:
	case LINUX32_SYS_linux_timer_create:
	case LINUX32_SYS_linux_clock_gettime64:
	case LINUX32_SYS_linux_clock_settime64:
	case LINUX32_SYS_linux_clock_getres_time64:
		putchar('(');
		sysdecode_linux_clockid(stdout, *ip);
		c = ',';
		ip++;
		narg--;
		break;
	case LINUX32_SYS_linux_kill:
	case LINUX32_SYS_linux_tkill:
	case LINUX32_SYS_linux_rt_sigqueueinfo:
		print_number(ip, narg, c);
		putchar(',');
		print_linux_signal(*ip);
		ip++;
		narg--;
		break;
	case LINUX32_SYS_linux_tgkill:
	case LINUX32_SYS_linux_rt_tgsigqueueinfo:
		print_number(ip, narg, c);
		print_number(ip, narg, c);
		putchar(',');
		print_linux_signal(*ip);
		ip++;
		narg--;
		break;
	case LINUX32_SYS_linux_signal:
	case LINUX32_SYS_linux_sigaction:
	case LINUX32_SYS_linux_rt_sigaction:
		putchar('(');
		print_linux_signal(*ip);
		ip++;
		narg--;
		c = ',';
		break;
	case LINUX32_SYS_linux_ftruncate:
	case LINUX32_SYS_linux_truncate:
		print_number(ip, narg, c);
		print_number64(first, ip, narg, c);
		break;
	}
	*resc = c;
	*resip = ip;
	*resnarg = narg;
}
#endif /* __amd64__ */
