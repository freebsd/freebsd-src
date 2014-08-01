/*	$NetBSD: cpufunc_asm_xscale.S,v 1.16 2002/08/17 16:36:32 thorpej Exp $	*/

/*-
 * Copyright (c) 2007 Olivier Houchard
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen Briggs and Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*-
 * Copyright (c) 2001 Matt Thomas.
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
 * XScale core 3 assembly functions for CPU / MMU / TLB specific operations
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * Size of the XScale core D-cache.
 */
#define	DCACHE_SIZE		0x00008000

.Lblock_userspace_access:
	.word	_C_LABEL(block_userspace_access)

/*
 * CPWAIT -- Canonical method to wait for CP15 update.
 * From: Intel 80200 manual, section 2.3.3.
 *
 * NOTE: Clobbers the specified temp reg.
 */
#define	CPWAIT_BRANCH							 \
	sub	pc, pc, #4

#define	CPWAIT(tmp)							 \
	mrc	p15, 0, tmp, c2, c0, 0	/* arbitrary read of CP15 */	;\
	mov	tmp, tmp		/* wait for it to complete */	;\
	CPWAIT_BRANCH			/* branch to next insn */

#define	CPWAIT_AND_RETURN_SHIFTER	lsr #32

#define	CPWAIT_AND_RETURN(tmp)						 \
	mrc	p15, 0, tmp, c2, c0, 0	/* arbitrary read of CP15 */	;\
	/* Wait for it to complete and branch to the return address */	 \
	sub	pc, lr, tmp, CPWAIT_AND_RETURN_SHIFTER

#define ARM_USE_L2_CACHE

#define L2_CACHE_SIZE		0x80000
#define L2_CACHE_WAYS		8
#define L2_CACHE_LINE_SIZE	32
#define L2_CACHE_SETS		(L2_CACHE_SIZE / \
    (L2_CACHE_WAYS * L2_CACHE_LINE_SIZE))

#define L1_DCACHE_SIZE		32 * 1024
#define L1_DCACHE_WAYS		4
#define L1_DCACHE_LINE_SIZE	32
#define L1_DCACHE_SETS		(L1_DCACHE_SIZE / \
    (L1_DCACHE_WAYS * L1_DCACHE_LINE_SIZE))
#ifdef CACHE_CLEAN_BLOCK_INTR
#define	XSCALE_CACHE_CLEAN_BLOCK					\
	stmfd	sp!, {r4}					;	\
	mrs	r4, cpsr					;	\
	orr	r0, r4, #(I32_bit | F32_bit)			;	\
	msr	cpsr_fsxc, r0

#define	XSCALE_CACHE_CLEAN_UNBLOCK					\
	msr	cpsr_fsxc, r4					;	\
	ldmfd	sp!, {r4}
#else
#define	XSCALE_CACHE_CLEAN_BLOCK					\
	stmfd	sp!, {r4}					;	\
	ldr	r4, .Lblock_userspace_access			;	\
	ldr	ip, [r4]					;	\
	orr	r0, ip, #1					;	\
	str	r0, [r4]	

#define	XSCALE_CACHE_CLEAN_UNBLOCK					\
	str	ip, [r3]					;	\
	ldmfd	sp!, {r4}
#endif /* CACHE_CLEAN_BLOCK_INTR */


ENTRY_NP(xscalec3_cache_syncI)
xscalec3_cache_purgeID:
EENTRY_NP(xscalec3_cache_purgeID)
	mcr	p15, 0, r0, c7, c5, 0	/* flush I cache (D cleaned below) */
EENTRY_NP(xscalec3_cache_cleanID)
EENTRY_NP(xscalec3_cache_purgeD)
EENTRY(xscalec3_cache_cleanD)

	XSCALE_CACHE_CLEAN_BLOCK
	mov	r0, #0
1:
	mov	r1, r0, asl #30
	mov	r2, #0
2:
	orr	r3, r1, r2, asl #5
	mcr	p15, 0, r3, c7, c14, 2	/* clean and invalidate */
	add	r2, r2, #1
	cmp	r2, #L1_DCACHE_SETS
	bne	2b
	add	r0, r0, #1
	cmp	r0, #4
	bne	1b
	CPWAIT(r0)
	XSCALE_CACHE_CLEAN_UNBLOCK
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */

	RET
EEND(xscalec3_cache_purgeID)
EEND(xscalec3_cache_cleanID)
EEND(xscalec3_cache_purgeD)
EEND(xscalec3_cache_cleanD)
END(xscalec3_cache_syncI)

ENTRY(xscalec3_cache_purgeID_rng)

	cmp	r1, #0x4000
	bcs	_C_LABEL(xscalec3_cache_cleanID)
	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 0, r0, c7, c14, 1	/* clean/invalidate L1 D cache entry */
	nop
	mcr	p15, 0, r0, c7, c5, 1	/* flush I cache single entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	CPWAIT(r0)

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)
END(xscalec3_cache_purgeID_rng)

ENTRY(xscalec3_cache_syncI_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(xscalec3_cache_syncI)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 0, r0, c7, c10, 1	/* clean D cache entry */
	mcr	p15, 0, r0, c7, c5, 1	/* flush I cache single entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	CPWAIT(r0)

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)
END(xscalec3_cache_syncI_rng)
	
ENTRY(xscalec3_cache_purgeD_rng)

	cmp	r1, #0x4000
	bcs	_C_LABEL(xscalec3_cache_cleanID)
	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 0, r0, c7, c14, 1	/* Clean and invalidate D cache entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	CPWAIT(r0)

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)
END(xscalec3_cache_purgeD_rng)

ENTRY(xscalec3_cache_cleanID_rng)
EENTRY(xscalec3_cache_cleanD_rng)

	cmp	r1, #0x4000
	bcs	_C_LABEL(xscalec3_cache_cleanID)
	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 0, r0, c7, c10, 1	/* clean L1 D cache entry */
	nop
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	CPWAIT(r0)

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)
EEND(xscalec3_cache_cleanD_rng)
END(xscalec3_cache_cleanID_rng)

ENTRY(xscalec3_l2cache_purge)
	/* Clean-up the L2 cache */
	mcr	p15, 0, r0, c7, c10, 5	/* Data memory barrier */
	mov	r0, #0
1:
	mov	r1, r0, asl #29
	mov	r2, #0
2:
	orr	r3, r1, r2, asl #5
	mcr	p15, 1, r3, c7, c15, 2
	add	r2, r2, #1
	cmp	r2, #L2_CACHE_SETS
	bne	2b
	add	r0, r0, #1
	cmp	r0, #8
	bne	1b
	mcr	p15, 0, r0, c7, c10, 4		@ data write barrier

	CPWAIT(r0)
	mcr	p15, 0, r0, c7, c10, 5	/* Data memory barrier */
	RET
END(xscalec3_l2cache_purge)

ENTRY(xscalec3_l2cache_clean_rng)
	mcr	p15, 0, r0, c7, c10, 5	/* Data memory barrier */

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 1, r0, c7, c11, 1	/* Clean L2 D cache entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b


	CPWAIT(r0)

	mcr	p15, 0, r0, c7, c10, 4		@ data write barrier
	mcr	p15, 0, r0, c7, c10, 5

	CPWAIT_AND_RETURN(r0)
END(xscalec3_l2cache_clean_rng)

ENTRY(xscalec3_l2cache_purge_rng)

	mcr	p15, 0, r0, c7, c10, 5	/* Data memory barrier */

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 1, r0, c7, c11, 1	/* Clean L2 D cache entry */
	mcr	p15, 1, r0, c7, c7, 1   /* Invalidate L2 D cache entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	mcr	p15, 0, r0, c7, c10, 4		@ data write barrier
	mcr	p15, 0, r0, c7, c10, 5

	CPWAIT_AND_RETURN(r0)
END(xscalec3_l2cache_purge_rng)

ENTRY(xscalec3_l2cache_flush_rng)
	mcr	p15, 0, r0, c7, c10, 5	/* Data memory barrier */

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	p15, 1, r0, c7, c7, 1   /* Invalidate L2 cache line */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b
	mcr	p15, 0, r0, c7, c10, 4		@ data write barrier
	mcr	p15, 0, r0, c7, c10, 5
	CPWAIT_AND_RETURN(r0)
END(xscalec3_l2cache_flush_rng)

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(xscalec3_setttb)
#ifdef CACHE_CLEAN_BLOCK_INTR
	mrs	r3, cpsr
	orr	r1, r3, #(I32_bit | F32_bit)
	msr	cpsr_fsxc, r1
#else
	ldr	r3, .Lblock_userspace_access
	ldr	r2, [r3]
	orr	r1, r2, #1
	str	r1, [r3]
#endif
	stmfd	sp!, {r0-r3, lr}
	bl	_C_LABEL(xscalec3_cache_cleanID)
	mcr	p15, 0, r0, c7, c5, 0	/* invalidate I$ and BTB */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write and fill buffer */

	CPWAIT(r0)

	ldmfd	sp!, {r0-r3, lr}

#ifdef ARM_USE_L2_CACHE
	orr	r0, r0, #0x18	/* cache the page table in L2 */
#endif
	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLB */

	CPWAIT(r0)

#ifdef CACHE_CLEAN_BLOCK_INTR
	msr	cpsr_fsxc, r3
#else
	str	r2, [r3]
#endif
	RET
END(xscalec3_setttb)

/*
 * Context switch.
 *
 * These is the CPU-specific parts of the context switcher cpu_switch()
 * These functions actually perform the TTB reload.
 *
 * NOTE: Special calling convention
 *	r1, r4-r13 must be preserved
 */
ENTRY(xscalec3_context_switch)
	/*
	 * CF_CACHE_PURGE_ID will *ALWAYS* be called prior to this.
	 * Thus the data cache will contain only kernel data and the
	 * instruction cache will contain only kernel code, and all
	 * kernel mappings are shared by all processes.
	 */
#ifdef ARM_USE_L2_CACHE
	orr	r0, r0, #0x18	/* Cache the page table in L2 */
#endif
	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c8, c7, 0	/* flush the I+D tlb */

	CPWAIT_AND_RETURN(r0)
END(xscalec3_context_switch)

