/*-
 * Copyright (c) 1998 Doug Rabson
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

#ifndef _MACHINE_CPUFUNC_H_
#define _MACHINE_CPUFUNC_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <machine/ia64_cpu.h>
#include <machine/vmparam.h>

#ifndef _SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

struct thread;

#define	IA64_FIXED_BREAK	0x84B5D

#ifdef __GNUCLIKE_ASM

static __inline void
breakpoint(void)
{
	__asm __volatile("break.m %0" :: "i"(IA64_FIXED_BREAK));
}

#define	HAVE_INLINE_FFS
#define	ffs(x)	__builtin_ffs(x)


static __inline void
ia64_disable_intr(void)
{
	__asm __volatile ("rsm psr.i");
}

static __inline void
ia64_enable_intr(void)
{
	__asm __volatile ("ssm psr.i;; srlz.d");
}

static __inline register_t
intr_disable(void)
{
	register_t psr;

	__asm __volatile ("mov %0=psr;;" : "=r"(psr));
	ia64_disable_intr();
	return ((psr & IA64_PSR_I) ? 1 : 0);
}

static __inline void
intr_restore(register_t ie)
{
	if (ie)
		ia64_enable_intr();
}

#endif /* __GNUCLIKE_ASM */

#endif /* _KERNEL */

#endif /* !_MACHINE_CPUFUNC_H_ */
