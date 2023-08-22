/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Dmitry Chagin <dchagin@FreeBSD.org>
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
#include <sys/elf.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/stddef.h>
#define	_KERNEL
#include <sys/vdso.h>
#undef	_KERNEL
#include <stdbool.h>

#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/stdarg.h>

#include <amd64/linux/linux.h>
#include <amd64/linux/linux_syscall.h>
#include <compat/linux/linux_errno.h>
#include <compat/linux/linux_time.h>

/* The kernel fixup this at vDSO install */
uintptr_t *kern_timekeep_base = NULL;
uint32_t kern_tsc_selector = 0;
uint32_t kern_cpu_selector = 0;

#include <x86/linux/linux_vdso_gettc_x86.inc>
#include <x86/linux/linux_vdso_getcpu_x86.inc>

/* for debug purpose */
static int
write(int fd, const void *buf, size_t size)
{
	int res;

	__asm__ __volatile__
	(
	    "syscall"
	    : "=a"(res)
	    : "a"(LINUX_SYS_write), "D"(fd), "S"(buf), "d"(size)
	    : "cc", "rcx", "r11", "memory"
	);
	return (res);
}

static int
__vdso_clock_gettime_fallback(clockid_t clock_id, struct l_timespec *ts)
{
	int res;

	__asm__ __volatile__
	(
	    "syscall"
	    : "=a"(res)
	    : "a"(LINUX_SYS_linux_clock_gettime), "D"(clock_id), "S"(ts)
	    : "cc", "rcx", "r11", "memory"
	);
	return (res);
}

static int
__vdso_gettimeofday_fallback(l_timeval *tv, struct timezone *tz)
{
	int res;

	__asm__ __volatile__
	(
	    "syscall"
	    : "=a"(res)
	    : "a"(LINUX_SYS_gettimeofday), "D"(tv), "S"(tz)
	    : "cc", "rcx", "r11", "memory"
	);
	return (res);
}

static int
__vdso_clock_getres_fallback(clockid_t clock_id, struct l_timespec *ts)
{
	int res;

	__asm__ __volatile__
	(
	    "syscall"
	    : "=a"(res)
	    : "a"(LINUX_SYS_linux_clock_getres), "D"(clock_id), "S"(ts)
	    : "cc", "rcx", "r11", "memory"
	);
	return (res);
}

static int
__vdso_getcpu_fallback(uint32_t *cpu, uint32_t *node, void *cache)
{
	int res;

	__asm__ __volatile__
	(
	    "syscall"
	    : "=a"(res)
	    : "a"(LINUX_SYS_linux_getcpu), "D"(cpu), "S"(node), "d"(cache)
	    : "cc", "rcx", "r11", "memory"
	);
	return (res);
}

static int
__vdso_time_fallback(long *tm)
{
	int res;

	__asm__ __volatile__
	(
	    "syscall"
	    : "=a"(res)
	    : "a"(LINUX_SYS_linux_time), "D"(tm)
	    : "cc", "rcx", "r11", "memory"
	);
	return (res);
}

#include <compat/linux/linux_vdso_gtod.inc>
