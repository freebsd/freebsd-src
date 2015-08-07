/*-
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
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
#include <sys/elf.h>
#include <sys/time.h>
#include <sys/vdso.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>
#include "libc_private.h"

static int lfence_works = -1;

static int
get_lfence_usage(void)
{
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
		    : "=r" (cpuid_supported) : : "eax", "ecx");
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
	return (lfence_works);
}

static u_int
__vdso_gettc_low(const struct vdso_timehands *th)
{
	u_int rv;

	if (get_lfence_usage() == 1)
		lfence();
	__asm __volatile("rdtsc; shrd %%cl, %%edx, %0"
	    : "=a" (rv) : "c" (th->th_x86_shift) : "edx");
	return (rv);
}

static u_int
__vdso_rdtsc32(void)
{
	u_int rv;

	if (get_lfence_usage() == 1)
		lfence();
	rv = rdtsc32();
	return (rv);
}

#pragma weak __vdso_gettc
u_int
__vdso_gettc(const struct vdso_timehands *th)
{

	return (th->th_x86_shift > 0 ? __vdso_gettc_low(th) :
	    __vdso_rdtsc32());
}

#pragma weak __vdso_gettimekeep
int
__vdso_gettimekeep(struct vdso_timekeep **tk)
{

	return (_elf_aux_info(AT_TIMEKEEP, tk, sizeof(*tk)));
}
