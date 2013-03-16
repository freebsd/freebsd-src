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
#include <machine/asi.h>
#include <machine/asmacros.h>

extern char pcpu0[1];

static inline void
counter_u64_inc(counter_u64_t c, uint64_t inc)
{

	__asm __volatile(
	"add	%" __XSTRING(PCPU_REG) ", %0, %%g1\n\t"
	"ldx	[%%g1], %%g3\n\t"
"1:\n\t"
	"mov	%%g3, %%g2\n\t"
	"add	%%g2, %1, %%g3\n\t"
	"casxa	[%%g1] " __XSTRING(ASI_N) " , %%g2, %%g3\n\t"
	"bne,pn	%%xcc, 1b\n\t"
	" cmp	%%g2, %%g3\n\t"
	:
	: "r" ((char *)c - pcpu0), "r" (inc)
	: "memory", "cc", "g1", "g2", "g3");

#if 0
	__asm __volatile(
	"wrpr	%%g0, %2, %%pstate\n\t"
	"ldx	[%" __XSTRING(PCPU_REG) " + %0], %%g2\n\t"
	"add	%%g2, %1, %%g1\n\t"
	"stx	%%g1, [%" __XSTRING(PCPU_REG) " + %0]\n\t"
	"wrpr	%%g0, %3, %%pstate\n\t"
	:
	: "r" ((char *)c - pcpu0), "r" (inc),
	  "i" (PSTATE_NORMAL), "i" (PSTATE_KERNEL)
	: "memory", "cc", "g2", "g1");
#endif
}

static inline void
counter_u64_dec(counter_u64_t c, uint64_t dec)
{

	critical_enter();
	*(uint64_t *)((char *)c + sizeof(struct pcpu) * curcpu) -= dec;
	critical_exit();
}

#endif	/* ! __MACHINE_COUNTER_H__ */
