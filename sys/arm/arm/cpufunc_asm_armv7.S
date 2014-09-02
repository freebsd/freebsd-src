/*-
 * Copyright (c) 2010 Per Odlund <per.odlund@armagedon.se>
 * Copyright (C) 2011 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

	.cpu cortex-a8

.Lcoherency_level:
	.word	_C_LABEL(arm_cache_loc)
.Lcache_type:
	.word	_C_LABEL(arm_cache_type)
.Lway_mask:
	.word	0x3ff
.Lmax_index:
	.word	0x7fff
.Lpage_mask:
	.word	0xfff

#define PT_NOS          (1 << 5)
#define PT_S 	        (1 << 1)
#define PT_INNER_NC	0
#define PT_INNER_WT	(1 << 0)
#define PT_INNER_WB	((1 << 0) | (1 << 6))
#define PT_INNER_WBWA	(1 << 6)
#define PT_OUTER_NC	0
#define PT_OUTER_WT	(2 << 3)
#define PT_OUTER_WB	(3 << 3)
#define PT_OUTER_WBWA	(1 << 3)
	
#ifdef SMP
#define PT_ATTR	(PT_S|PT_INNER_WBWA|PT_OUTER_WBWA|PT_NOS)
#else
#define PT_ATTR	(PT_INNER_WBWA|PT_OUTER_WBWA)
#endif

ENTRY(armv7_setttb)
	stmdb   sp!, {r0, lr}
 	bl      _C_LABEL(armv7_idcache_wbinv_all) /* clean the D cache */
 	ldmia   sp!, {r0, lr}
 	dsb
				
	orr 	r0, r0, #PT_ATTR
 	mcr	p15, 0, r0, c2, c0, 0	/* Translation Table Base Register 0 (TTBR0) */
	isb
#ifdef SMP
 	mcr     p15, 0, r0, c8, c3, 0   /* invalidate I+D TLBs Inner Shareable*/
#else
 	mcr     p15, 0, r0, c8, c7, 0   /* invalidate I+D TLBs */
#endif
 	dsb
 	isb
	RET
END(armv7_setttb)

ENTRY(armv7_tlb_flushID)
	dsb
#ifdef SMP
	mcr	p15, 0, r0, c8, c3, 0	/* flush Unified TLB all entries Inner Shareable */
	mcr	p15, 0, r0, c7, c1, 6	/* flush BTB Inner Shareable */
#else
	mcr	p15, 0, r0, c8, c7, 0	/* flush Unified TLB all entries */
	mcr	p15, 0, r0, c7, c5, 6	/* flush BTB */
#endif
	dsb
	isb
	mov	pc, lr
END(armv7_tlb_flushID)

ENTRY(armv7_tlb_flushID_SE)
	ldr	r1, .Lpage_mask
	bic	r0, r0, r1
#ifdef SMP
	mcr	p15, 0, r0, c8, c3, 3	/* flush Unified TLB single entry Inner Shareable */
	mcr	p15, 0, r0, c7, c1, 6	/* flush BTB Inner Shareable */
#else
	mcr	p15, 0, r0, c8, c7, 1	/* flush Unified TLB single entry */
	mcr	p15, 0, r0, c7, c5, 6	/* flush BTB */
#endif
	dsb
	isb
	mov	pc, lr
END(armv7_tlb_flushID_SE)

/* Based on algorithm from ARM Architecture Reference Manual */
ENTRY(armv7_dcache_wbinv_all)
	stmdb	sp!, {r4, r5, r6, r7, r8, r9}

	/* Get cache level */
	ldr	r0, .Lcoherency_level
	ldr	r3, [r0]
	cmp	r3, #0
	beq	Finished
	/* For each cache level */
	mov	r8, #0
Loop1:
	/* Get cache type for given level */
	mov	r2, r8, lsl #2
	add	r2, r2, r2
	ldr	r0, .Lcache_type
	ldr	r1, [r0, r2]

	/* Get line size */
	and	r2, r1, #7
	add	r2, r2, #4

	/* Get number of ways */
	ldr	r4, .Lway_mask
	ands	r4, r4, r1, lsr #3
	clz	r5, r4

	/* Get max index */
	ldr	r7, .Lmax_index
	ands	r7, r7, r1, lsr #13
Loop2:
	mov	r9, r4
Loop3:
	mov	r6, r8, lsl #1
	orr	r6, r6, r9, lsl r5
	orr	r6, r6, r7, lsl r2

	/* Clean and invalidate data cache by way/index */
	mcr	p15, 0, r6, c7, c14, 2
	subs	r9, r9, #1
	bge	Loop3
	subs	r7, r7, #1
	bge	Loop2
Skip:
	add	r8, r8, #1
	cmp	r3, r8
	bne Loop1
Finished:
	dsb
	ldmia	sp!, {r4, r5, r6, r7, r8, r9}
	RET
END(armv7_dcache_wbinv_all)

ENTRY(armv7_idcache_wbinv_all)
	stmdb	sp!, {lr}
	bl armv7_dcache_wbinv_all
#ifdef SMP
	mcr	p15, 0, r0, c7, c1, 0	/* Invalidate all I caches to PoU (ICIALLUIS) */
#else
	mcr	p15, 0, r0, c7, c5, 0	/* Invalidate all I caches to PoU (ICIALLU) */
#endif
	dsb
	isb
	ldmia	sp!, {lr}
	RET
END(armv7_idcache_wbinv_all)

/* XXX Temporary set it to 32 for MV cores, however this value should be
 * get from Cache Type register
 */
.Larmv7_line_size:
	.word	32

ENTRY(armv7_dcache_wb_range)
	ldr	ip, .Larmv7_line_size
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larmv7_wb_next:
	mcr	p15, 0, r0, c7, c10, 1	/* Clean D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larmv7_wb_next
	dsb				/* data synchronization barrier */
	RET
END(armv7_dcache_wb_range)

ENTRY(armv7_dcache_wbinv_range)
	ldr	ip, .Larmv7_line_size
	sub     r3, ip, #1
	and     r2, r0, r3
	add     r1, r1, r2
	bic     r0, r0, r3
.Larmv7_wbinv_next:
	mcr	p15, 0, r0, c7, c14, 1	/* Purge D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larmv7_wbinv_next
	dsb				/* data synchronization barrier */
	RET
END(armv7_dcache_wbinv_range)

/*
 * Note, we must not invalidate everything.  If the range is too big we
 * must use wb-inv of the entire cache.
 */
ENTRY(armv7_dcache_inv_range)
	ldr	ip, .Larmv7_line_size
	sub     r3, ip, #1
	and     r2, r0, r3
	add     r1, r1, r2
	bic     r0, r0, r3
.Larmv7_inv_next:
	mcr	p15, 0, r0, c7, c6, 1	/* Invalidate D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larmv7_inv_next
	dsb				/* data synchronization barrier */
	RET
END(armv7_dcache_inv_range)

ENTRY(armv7_idcache_wbinv_range)
	ldr	ip, .Larmv7_line_size
	sub     r3, ip, #1
	and     r2, r0, r3
	add     r1, r1, r2
	bic     r0, r0, r3
.Larmv7_id_wbinv_next:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c14, 1	/* Purge D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larmv7_id_wbinv_next
	isb				/* instruction synchronization barrier */
	dsb				/* data synchronization barrier */
	RET
END(armv7_idcache_wbinv_range)

ENTRY_NP(armv7_icache_sync_all)
#ifdef SMP
	mcr	p15, 0, r0, c7, c1, 0	/* Invalidate all I cache to PoU Inner Shareable */
#else
	mcr	p15, 0, r0, c7, c5, 0	/* Invalidate all I cache to PoU (ICIALLU) */
#endif
	isb				/* instruction synchronization barrier */
	dsb				/* data synchronization barrier */
	RET
END(armv7_icache_sync_all)

ENTRY_NP(armv7_icache_sync_range)
	ldr	ip, .Larmv7_line_size
.Larmv7_sync_next:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c10, 1	/* Clean D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larmv7_sync_next
	isb				/* instruction synchronization barrier */
	dsb				/* data synchronization barrier */
	RET
END(armv7_icache_sync_range)

ENTRY(armv7_cpu_sleep)
	dsb				/* data synchronization barrier */
	wfi  				/* wait for interrupt */
	RET
END(armv7_cpu_sleep)

ENTRY(armv7_context_switch)
	dsb
	orr     r0, r0, #PT_ATTR
			
	mcr	p15, 0, r0, c2, c0, 0	/* set the new TTB */
	isb
#ifdef SMP
	mcr	p15, 0, r0, c8, c3, 0	/* and flush the I+D tlbs Inner Sharable */
#else
	mcr	p15, 0, r0, c8, c7, 0	/* and flush the I+D tlbs */
#endif
	dsb
	isb
	RET
END(armv7_context_switch)

ENTRY(armv7_drain_writebuf)
	dsb
	RET
END(armv7_drain_writebuf)

ENTRY(armv7_sev)
	dsb
	sev
	nop
	RET
END(armv7_sev)

ENTRY(armv7_auxctrl)
	mrc p15, 0, r2, c1, c0, 1
	bic r3, r2, r0	/* Clear bits */
	eor r3, r3, r1  /* XOR bits */

	teq r2, r3
	mcrne p15, 0, r3, c1, c0, 1
	mov r0, r2
	RET
END(armv7_auxctrl)

/*
 * Invalidate all I+D+branch cache.  Used by startup code, which counts
 * on the fact that only r0-r3,ip are modified and no stack space is used.
 */
ENTRY(armv7_idcache_inv_all)
	mov     r0, #0
	mcr     p15, 2, r0, c0, c0, 0   @ set cache level to L1
	mrc     p15, 1, r0, c0, c0, 0   @ read CCSIDR

	ubfx    r2, r0, #13, #15        @ get num sets - 1 from CCSIDR
	ubfx    r3, r0, #3, #10         @ get numways - 1 from CCSIDR
	clz     r1, r3                  @ number of bits to MSB of way
	lsl     r3, r3, r1              @ shift into position
	mov     ip, #1                  @
	lsl     ip, ip, r1              @ ip now contains the way decr

	ubfx    r0, r0, #0, #3          @ get linesize from CCSIDR
	add     r0, r0, #4              @ apply bias
	lsl     r2, r2, r0              @ shift sets by log2(linesize)
	add     r3, r3, r2              @ merge numsets - 1 with numways - 1
	sub     ip, ip, r2              @ subtract numsets - 1 from way decr
	mov     r1, #1
	lsl     r1, r1, r0              @ r1 now contains the set decr
	mov     r2, ip                  @ r2 now contains set way decr

	/* r3 = ways/sets, r2 = way decr, r1 = set decr, r0 and ip are free */
1:      mcr     p15, 0, r3, c7, c6, 2   @ invalidate line
	movs    r0, r3                  @ get current way/set
	beq     2f                      @ at 0 means we are done.
	movs    r0, r0, lsl #10         @ clear way bits leaving only set bits
	subne   r3, r3, r1              @ non-zero?, decrement set #
	subeq   r3, r3, r2              @ zero?, decrement way # and restore set count
	b       1b

2:	dsb                             @ wait for stores to finish
	mov     r0, #0                  @ and ...
	mcr     p15, 0, r0, c7, c5, 0   @ invalidate instruction+branch cache
	isb                             @ instruction sync barrier
	bx      lr                      @ return
END(armv7_idcache_inv_all)

ENTRY_NP(armv7_sleep)
	dsb
	wfi
	bx	lr
END(armv7_sleep)

