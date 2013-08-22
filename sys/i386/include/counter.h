/*-
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
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

#ifndef __MACHINE_COUNTER_H__
#define __MACHINE_COUNTER_H__

#include <sys/pcpu.h>
#ifdef INVARIANTS
#include <sys/proc.h>
#endif
#include <machine/md_var.h>
#include <machine/specialreg.h>

#define	counter_enter()	do {				\
	if ((cpu_feature & CPUID_CX8) == 0)		\
		critical_enter();			\
} while (0)

#define	counter_exit()	do {				\
	if ((cpu_feature & CPUID_CX8) == 0)		\
		critical_exit();			\
} while (0)

extern struct pcpu __pcpu[MAXCPU];

static inline void
counter_64_inc_8b(uint64_t *p, int64_t inc)
{

	__asm __volatile(
	"movl	%%fs:(%%esi),%%eax\n\t"
	"movl	%%fs:4(%%esi),%%edx\n"
"1:\n\t"
	"movl	%%eax,%%ebx\n\t"
	"movl	%%edx,%%ecx\n\t"
	"addl	(%%edi),%%ebx\n\t"
	"adcl	4(%%edi),%%ecx\n\t"
	"cmpxchg8b %%fs:(%%esi)\n\t"
	"jnz	1b"
	:
	: "S" ((char *)p - (char *)&__pcpu[0]), "D" (&inc)
	: "memory", "cc", "eax", "edx", "ebx", "ecx");
}

#define	counter_u64_add_protected(c, inc)	do {	\
	if ((cpu_feature & CPUID_CX8) == 0) {		\
		CRITICAL_ASSERT(curthread);		\
		*(uint64_t *)zpcpu_get(c) += (inc);	\
	} else						\
		counter_64_inc_8b((c), (inc));		\
} while (0)

static inline void
counter_u64_add(counter_u64_t c, int64_t inc)
{

	if ((cpu_feature & CPUID_CX8) == 0) {
		critical_enter();
		*(uint64_t *)zpcpu_get(c) += inc;
		critical_exit();
	} else {
		counter_64_inc_8b(c, inc);
	}
}

#endif	/* ! __MACHINE_COUNTER_H__ */
