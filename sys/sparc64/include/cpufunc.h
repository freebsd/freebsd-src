/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#ifndef	_MACHINE_CPUFUNC_H_
#define	_MACHINE_CPUFUNC_H_

#include <machine/asi.h>
#include <machine/pstate.h>

/*
 * membar operand macros for use in other macros when # is a special
 * character.  Keep these in sync with what the hardware expects.
 */
#define	C_Lookaside	(0)
#define	C_MemIssue	(1)
#define	C_Sync		(2)
#define	M_LoadLoad	(0)
#define	M_StoreLoad	(1)
#define	M_LoadStore	(2)
#define	M_StoreStore	(3)

#define	CMASK_SHIFT	(4)
#define	MMASK_SHIFT	(0)

#define	CMASK_GEN(bit)	((1 << (bit)) << CMASK_SHIFT)
#define	MMASK_GEN(bit)	((1 << (bit)) << MMASK_SHIFT)

#define	Lookaside	CMASK_GEN(C_Lookaside)
#define	MemIssue	CMASK_GEN(C_MemIssue)
#define	Sync		CMASK_GEN(C_Sync)
#define	LoadLoad	MMASK_GEN(M_LoadLoad)
#define	StoreLoad	MMASK_GEN(M_StoreLoad)
#define	LoadStore	MMASK_GEN(M_LoadStore)
#define	StoreStore	MMASK_GEN(M_StoreStore)

#define	casa(rs1, rs2, rd, asi) ({					\
	u_int __rd = (u_int32_t)(rd);					\
	__asm __volatile("casa [%1] %2, %3, %0"				\
	    : "+r" (__rd) : "r" (rs1), "n" (asi), "r" (rs2));		\
	__rd;								\
})

#define	casxa(rs1, rs2, rd, asi) ({					\
	u_long __rd = (u_int64_t)(rd);					\
	__asm __volatile("casxa [%1] %2, %3, %0"			\
	    : "+r" (__rd) : "r" (rs1), "n" (asi), "r" (rs2));		\
	__rd;								\
})

#define	flush(va) do {							\
	__asm __volatile("flush %0" : : "r" (va));			\
} while (0)

#define	flushw() do {							\
	__asm __volatile("flushw" : :);					\
} while (0)

#define	ldxa(va, asi) ({						\
	u_long __r;							\
	__asm __volatile("ldxa [%1] %2, %0"				\
	    : "=r" (__r) : "r" (va), "n" (asi));			\
	__r;								\
})

#define	stxa(va, asi, val) do {						\
	__asm __volatile("stxa %0, [%1] %2"				\
	    : : "r" (val), "r" (va), "n" (asi));			\
} while (0)

#define	membar(mask) do {						\
	__asm __volatile("membar %0" : : "n" (mask));			\
} while (0)

#define	rd(name) ({							\
	u_int64_t __sr;							\
	__asm __volatile("rd %%" #name ", %0" : "=r" (__sr) :);		\
	__sr;								\
})

#define	wr(name, val, xor) do {						\
	__asm __volatile("wr %0, %1, %%" #name				\
	    : : "r" (val), "rI" (xor));					\
} while (0)

#define	rdpr(name) ({							\
	u_int64_t __pr;							\
	__asm __volatile("rdpr %%" #name", %0" : "=r" (__pr) :);	\
	__pr;								\
})

#define	wrpr(name, val, xor) do {					\
	__asm __volatile("wrpr %0, %1, %%" #name			\
	    : : "r" (val), "rI" (xor));					\
} while (0)

static __inline void
breakpoint(void)
{
	__asm __volatile("ta %%xcc, 1" : :);
}

static __inline critical_t
critical_enter(void)
{
	critical_t pil;

	pil = rdpr(pil);
	wrpr(pil, 0, 14);
	return (pil);
}

static __inline void
critical_exit(critical_t pil)
{
	wrpr(pil, pil, 0);
}

/*
 * Ultrasparc II doesn't implement popc in hardware.  Suck.
 */
#if 0
#define	HAVE_INLINE_FFS
/*
 * See page 202 of the SPARC v9 Architecture Manual.
 */
static __inline int
ffs(int mask)
{
	int result;
	int neg;
	int tmp;

	__asm __volatile(
	"	neg	%3, %1 ;	"
	"	xnor	%3, %1, %2 ;	"
	"	popc	%2, %0 ;	"
	"	movrz	%3, %%g0, %0 ;	"
	: "=r" (result), "=r" (neg), "=r" (tmp) : "r" (mask));
	return (result);
}
#endif

#endif /* !_MACHINE_CPUFUNC_H_ */
