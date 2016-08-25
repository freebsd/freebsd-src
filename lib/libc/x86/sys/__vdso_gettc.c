/*-
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
#include "namespace.h"
#include <sys/elf.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/vdso.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"
#include <machine/cpufunc.h>
#include <machine/specialreg.h>
#include <dev/acpica/acpi_hpet.h>
#include "libc_private.h"

static void
lfence_mb(void)
{
#if defined(__i386__)
	static int lfence_works = -1;
	u_int cpuid_supported, p[4];

	if (lfence_works == -1) {
		__asm __volatile(
		    "	pushfl\n"
		    "	popl	%%eax\n"
		    "	movl    %%eax,%%ecx\n"
		    "	xorl    $0x200000,%%eax\n"
		    "	pushl	%%eax\n"
		    "	popfl\n"
		    "	pushfl\n"
		    "	popl    %%eax\n"
		    "	xorl    %%eax,%%ecx\n"
		    "	je	1f\n"
		    "	movl	$1,%0\n"
		    "	jmp	2f\n"
		    "1:	movl	$0,%0\n"
		    "2:\n"
		    : "=r" (cpuid_supported) : : "eax", "ecx", "cc");
		if (cpuid_supported) {
			__asm __volatile(
			    "	pushl	%%ebx\n"
			    "	cpuid\n"
			    "	movl	%%ebx,%1\n"
			    "	popl	%%ebx\n"
			    : "=a" (p[0]), "=r" (p[1]), "=c" (p[2]), "=d" (p[3])
			    :  "0" (0x1));
			lfence_works = (p[3] & CPUID_SSE2) != 0;
		} else
			lfence_works = 0;
	}
	if (lfence_works == 1)
		lfence();
#elif defined(__amd64__)
	lfence();
#else
#error "arch"
#endif
}

static u_int
__vdso_gettc_rdtsc_low(const struct vdso_timehands *th)
{
	u_int rv;

	lfence_mb();
	__asm __volatile("rdtsc; shrd %%cl, %%edx, %0"
	    : "=a" (rv) : "c" (th->th_x86_shift) : "edx");
	return (rv);
}

static u_int
__vdso_rdtsc32(void)
{

	lfence_mb();
	return (rdtsc32());
}

static char *hpet_dev_map = NULL;
static uint32_t hpet_idx = 0xffffffff;

static void
__vdso_init_hpet(uint32_t u)
{
	static const char devprefix[] = "/dev/hpet";
	char devname[64], *c, *c1, t;
	int fd;

	c1 = c = stpcpy(devname, devprefix);
	u = hpet_idx;
	do {
		*c++ = u % 10 + '0';
		u /= 10;
	} while (u != 0);
	*c = '\0';
	for (c--; c1 != c; c1++, c--) {
		t = *c1;
		*c1 = *c;
		*c = t;
	}
	fd = _open(devname, O_RDONLY);
	if (fd == -1) {
		hpet_dev_map = MAP_FAILED;
		return;
	}
	if (hpet_dev_map != NULL && hpet_dev_map != MAP_FAILED)
		munmap(hpet_dev_map, PAGE_SIZE);
	hpet_dev_map = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	_close(fd);
}

#pragma weak __vdso_gettc
int
__vdso_gettc(const struct vdso_timehands *th, u_int *tc)
{
	uint32_t tmp;

	switch (th->th_algo) {
	case VDSO_TH_ALGO_X86_TSC:
		*tc = th->th_x86_shift > 0 ? __vdso_gettc_rdtsc_low(th) :
		    __vdso_rdtsc32();
		return (0);
	case VDSO_TH_ALGO_X86_HPET:
		tmp = th->th_x86_hpet_idx;
		if (hpet_dev_map == NULL || tmp != hpet_idx) {
			hpet_idx = tmp;
			__vdso_init_hpet(hpet_idx);
		}
		if (hpet_dev_map == MAP_FAILED)
			return (ENOSYS);
		*tc = *(volatile uint32_t *)(hpet_dev_map + HPET_MAIN_COUNTER);
		return (0);
	default:
		return (ENOSYS);
	}
}

#pragma weak __vdso_gettimekeep
int
__vdso_gettimekeep(struct vdso_timekeep **tk)
{

	return (_elf_aux_info(AT_TIMEKEEP, tk, sizeof(*tk)));
}
