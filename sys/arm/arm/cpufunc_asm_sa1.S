/*	$NetBSD: cpufunc_asm_sa1.S,v 1.8 2002/08/17 16:36:32 thorpej Exp $	*/

/*-
 * Copyright (c) 1997,1998 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Causality Limited.
 * 4. The name of Causality Limited may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUSALITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * SA-1 assembly functions for CPU / MMU / TLB specific operations
 *
 */
 
#include <machine/asm.h>
__FBSDID("$FreeBSD: src/sys/arm/arm/cpufunc_asm_sa1.S,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $");

.Lblock_userspace_access:
	.word	_C_LABEL(block_userspace_access)

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(getttb)
	mrc	p15, 0, r0, c2, c0, 0
ENTRY(sa1_setttb)
#ifdef CACHE_CLEAN_BLOCK_INTR
	mrs	r3, cpsr_all
	orr	r1, r3, #(I32_bit | F32_bit)
	msr	cpsr_all, r1
#else
	ldr	r3, .Lblock_userspace_access
	ldr	r2, [r3]
	orr	r1, r2, #1 
	str	r1, [r3]
#endif
	stmfd	sp!, {r0-r3, lr}
	bl	_C_LABEL(sa1_cache_cleanID)
	ldmfd	sp!, {r0-r3, lr}
	mcr	p15, 0, r0, c7, c5, 0	/* invalidate I$ and BTB */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write and fill buffer */

	/* Write the TTB */ 
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLB */

	/* The cleanID above means we only need to flush the I cache here */
	mcr	p15, 0, r0, c7, c5, 0	/* invalidate I$ and BTB */

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
#ifdef CACHE_CLEAN_BLOCK_INTR
	msr	cpsr_all, r3
#else
	str	r2, [r3]
#endif
	RET

/*
 * TLB functions
 */
ENTRY(sa1_tlb_flushID_SE)
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
	mcr	p15, 0, r0, c8, c5, 0	/* flush I tlb */
	RET

/*
 * Cache functions
 */
ENTRY(sa1_cache_flushID)
	mcr	p15, 0, r0, c7, c7, 0	/* flush I+D cache */
	RET

ENTRY(sa1_cache_flushI)
	mcr	p15, 0, r0, c7, c5, 0	/* flush I cache */
	RET

ENTRY(sa1_cache_flushD)
	mcr	p15, 0, r0, c7, c6, 0	/* flush D cache */
	RET

ENTRY(sa1_cache_flushD_SE)
	mcr	p15, 0, r0, c7, c6, 1	/* flush D cache single entry */
	RET

ENTRY(sa1_cache_cleanD_E)
	mcr	p15, 0, r0, c7, c10, 1	/* clean D cache entry */
	RET

/*
 * Information for the SA-1 cache clean/purge functions:
 *
 *	* Virtual address of the memory region to use
 *	* Size of memory region
 */
	.data

	.global	_C_LABEL(sa1_cache_clean_addr)
_C_LABEL(sa1_cache_clean_addr):
	.word	0xf0000000

	.global	_C_LABEL(sa1_cache_clean_size)
_C_LABEL(sa1_cache_clean_size):
#if defined(CPU_SA1100) || defined(CPU_SA1110)
	.word	0x00004000
#else
	.word	0x00008000
#endif

	.text

.Lsa1_cache_clean_addr:
	.word	_C_LABEL(sa1_cache_clean_addr)
.Lsa1_cache_clean_size:
	.word	_C_LABEL(sa1_cache_clean_size)

#ifdef CACHE_CLEAN_BLOCK_INTR
#define	SA1_CACHE_CLEAN_BLOCK						\
	mrs	r3, cpsr_all					;	\
	orr	r0, r3, #(I32_bit | F32_bit)			;	\
	msr	cpsr_all, r0

#define	SA1_CACHE_CLEAN_UNBLOCK						\
	msr	cpsr_all, r3
#else
#define	SA1_CACHE_CLEAN_BLOCK						\
	ldr	r3, .Lblock_userspace_access			;	\
	ldr	ip, [r3]					;	\
	orr	r0, ip, #1					;	\
	str	r0, [r3]

#define	SA1_CACHE_CLEAN_UNBLOCK						\
	str	ip, [r3]
#endif /* CACHE_CLEAN_BLOCK_INTR */

#ifdef DOUBLE_CACHE_CLEAN_BANK
#define	SA1_DOUBLE_CACHE_CLEAN_BANK					\
	eor	r0, r0, r1					;	\
	str	r0, [r2]
#else
#define	SA1_DOUBLE_CACHE_CLEAN_BANK	/* nothing */
#endif /* DOUBLE_CACHE_CLEAN_BANK */

#define	SA1_CACHE_CLEAN_PROLOGUE					\
	SA1_CACHE_CLEAN_BLOCK					;	\
	ldr	r2, .Lsa1_cache_clean_addr			;	\
	ldmia	r2, {r0, r1}					;	\
	SA1_DOUBLE_CACHE_CLEAN_BANK

#define	SA1_CACHE_CLEAN_EPILOGUE					\
	SA1_CACHE_CLEAN_UNBLOCK

ENTRY_NP(sa1_cache_syncI)
ENTRY_NP(sa1_cache_purgeID)
	mcr	p15, 0, r0, c7, c5, 0	/* flush I cache (D cleaned below) */
ENTRY_NP(sa1_cache_cleanID)
ENTRY_NP(sa1_cache_purgeD)
ENTRY(sa1_cache_cleanD)
	SA1_CACHE_CLEAN_PROLOGUE

1:	ldr	r2, [r0], #32
	subs	r1, r1, #32
	bne	1b

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */

	SA1_CACHE_CLEAN_EPILOGUE
	RET

ENTRY(sa1_cache_purgeID_E)
	mcr	p15, 0, r0, c7, c10, 1	/* clean dcache entry */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mcr	p15, 0, r0, c7, c5, 0	/* flush I cache */
	mcr	p15, 0, r0, c7, c6, 1	/* flush D cache single entry */
	RET

ENTRY(sa1_cache_purgeD_E)
	mcr	p15, 0, r0, c7, c10, 1	/* clean dcache entry */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mcr	p15, 0, r0, c7, c6, 1	/* flush D cache single entry */
	RET

/*
 * Soft functions
 */
/* sa1_cache_syncI is identical to sa1_cache_purgeID */

ENTRY(sa1_cache_cleanID_rng)
ENTRY(sa1_cache_cleanD_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(sa1_cache_cleanID)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 0, r0, c7, c10, 1	/* clean D cache entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET

ENTRY(sa1_cache_purgeID_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(sa1_cache_purgeID)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 0, r0, c7, c10, 1	/* clean D cache entry */
	mcr	p15, 0, r0, c7, c6, 1	/* flush D cache single entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mcr	p15, 0, r0, c7, c5, 0	/* flush I cache */
	RET

ENTRY(sa1_cache_purgeD_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(sa1_cache_purgeD)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 0, r0, c7, c10, 1	/* clean D cache entry */
	mcr	p15, 0, r0, c7, c6, 1	/* flush D cache single entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET

ENTRY(sa1_cache_syncI_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(sa1_cache_syncI)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 0, r0, c7, c10, 1	/* clean D cache entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mcr	p15, 0, r0, c7, c5, 0	/* flush I cache */

	RET

/*
 * Context switch.
 *
 * These is the CPU-specific parts of the context switcher cpu_switch()
 * These functions actually perform the TTB reload.
 *
 * NOTE: Special calling convention
 *	r1, r4-r13 must be preserved
 */
#if defined(CPU_SA110)
ENTRY(sa110_context_switch)
	/*
	 * CF_CACHE_PURGE_ID will *ALWAYS* be called prior to this.
	 * Thus the data cache will contain only kernel data and the
	 * instruction cache will contain only kernel code, and all
	 * kernel mappings are shared by all processes.
	 */

	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c8, c7, 0	/* flush the I+D tlb */

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
	RET
#endif
