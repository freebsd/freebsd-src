/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
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
#include <machine/stdarg.h>

#include <arm64/linux/linux.h>
#include <arm64/linux/linux_syscall.h>
#include <compat/linux/linux_errno.h>
#include <compat/linux/linux_time.h>

/* The kernel fixup this at vDSO install */
uintptr_t *kern_timekeep_base = NULL;
uint32_t kern_tsc_selector = 0;

static int
write(int lfd, const void *lbuf, size_t lsize)
{
	register long svc asm("x8") = LINUX_SYS_write;
	register int fd asm("x0") = lfd;
	register const char *buf asm("x1") = lbuf;
	register long size asm("x2") = lsize;
	register long res asm ("x0");

	asm volatile(
	"       svc #0\n"
	: "=r" (res)
	: "r" (fd), "r" (buf), "r" (size), "r" (svc)
	: "memory");
	return (res);
}

static int
__vdso_clock_gettime_fallback(clockid_t clock_id, struct l_timespec *lts)
{
	register long svc asm("x8") = LINUX_SYS_linux_clock_gettime;
	register clockid_t clockid asm("x0") = clock_id;
	register struct l_timespec *ts asm("x1") = lts;
	register long res asm ("x0");

	asm volatile(
	"       svc #0\n"
	: "=r" (res)
	: "r" (clockid), "r" (ts), "r" (svc)
	: "memory");
	return (res);
}

static int
__vdso_gettimeofday_fallback(l_timeval *ltv, struct timezone *ltz)
{
	register long svc asm("x8") = LINUX_SYS_gettimeofday;
	register l_timeval *tv asm("x0") = ltv;
	register struct timezone *tz asm("x1") = ltz;
	register long res asm ("x0");

	asm volatile(
	"       svc #0\n"
	: "=r" (res)
	: "r" (tv), "r" (tz), "r" (svc)
	: "memory");
	return (res);
}

static int
__vdso_clock_getres_fallback(clockid_t clock_id, struct l_timespec *lts)
{
	register long svc asm("x8") = LINUX_SYS_linux_clock_getres;
	register clockid_t clockid asm("x0") = clock_id;
	register struct l_timespec *ts asm("x1") = lts;
	register long res asm ("x0");

	asm volatile(
	"       svc #0\n"
	: "=r" (res)
	: "r" (clockid), "r" (ts), "r" (svc)
	: "memory");
	return (res);
}

/*
 * copied from lib/libc/aarch64/sys/__vdso_gettc.c
 */

static inline uint64_t
cp15_cntvct_get(void)
{
	uint64_t reg;

	__asm __volatile("mrs %0, cntvct_el0" : "=r" (reg));
	return (reg);
}

static inline uint64_t
cp15_cntpct_get(void)
{
	uint64_t reg;

	__asm __volatile("mrs %0, cntpct_el0" : "=r" (reg));
	return (reg);
}

int
__vdso_gettc(const struct vdso_timehands *th, u_int *tc)
{

	if (th->th_algo != VDSO_TH_ALGO_ARM_GENTIM)
		return (ENOSYS);
	__asm __volatile("isb" : : : "memory");
	*tc = th->th_physical == 0 ? cp15_cntvct_get() : cp15_cntpct_get();
	return (0);
}

#include <compat/linux/linux_vdso_gtod.inc>
