/*	$NetBSD: cpufunc_asm_arm9.S,v 1.3 2004/01/26 15:54:16 rearnsha Exp $	*/

/*
 * Copyright (c) 2001, 2004 ARM Limited
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
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * ARM9 assembly functions for CPU / MMU / TLB specific operations
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(arm9_setttb)
	stmfd	sp!, {r0, lr}
	bl	_C_LABEL(arm9_idcache_wbinv_all)
	ldmfd	sp!, {r0, lr}

	mcr	p15, 0, r0, c2, c0, 0	/* load new TTB */

	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLBs */
	mov	pc, lr
END(arm9_setttb)

/*
 * TLB functions
 */
ENTRY(arm9_tlb_flushID_SE)
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
	mov	pc, lr
END(arm9_tlb_flushID_SE)

/*
 * Cache operations.  For the entire cache we use the set/index
 * operations.
 */
	s_max	.req r0
	i_max	.req r1
	s_inc	.req r2
	i_inc	.req r3

ENTRY_NP(arm9_icache_sync_range)
	ldr	ip, .Larm9_line_size
	cmp	r1, #0x4000
	bcs	.Larm9_icache_sync_all
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm9_sync_next:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c10, 1	/* Clean D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm9_sync_next
	mov	pc, lr

.Larm9_icache_sync_all:
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache cleaning code.
	 */
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */
	/* Fall through to clean Dcache. */

.Larm9_dcache_wb:
	ldr	ip, .Larm9_cache_data
	ldmia	ip, {s_max, i_max, s_inc, i_inc}
.Lnext_set:
	orr	ip, s_max, i_max
.Lnext_index:
	mcr	p15, 0, ip, c7, c10, 2	/* Clean D cache SE with Set/Index */
	subs	ip, ip, i_inc
	bhs	.Lnext_index		/* Next index */
	subs	s_max, s_max, s_inc
	bhs	.Lnext_set		/* Next set */
	mov	pc, lr
END(arm9_icache_sync_range)

.Larm9_line_size:
	.word	_C_LABEL(arm_pdcache_line_size)

ENTRY(arm9_dcache_wb_range)
	ldr	ip, .Larm9_line_size
	cmp	r1, #0x4000
	bcs	.Larm9_dcache_wb
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm9_wb_next:
	mcr	p15, 0, r0, c7, c10, 1	/* Clean D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm9_wb_next
	mov	pc, lr
END(arm9_dcache_wb_range)

ENTRY(arm9_dcache_wbinv_range)
	ldr	ip, .Larm9_line_size
	cmp	r1, #0x4000
	bcs	.Larm9_dcache_wbinv_all
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm9_wbinv_next:
	mcr	p15, 0, r0, c7, c14, 1	/* Purge D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm9_wbinv_next
	mov	pc, lr
END(arm9_dcache_wbinv_range)

/*
 * Note, we must not invalidate everything.  If the range is too big we
 * must use wb-inv of the entire cache.
 */
ENTRY(arm9_dcache_inv_range)
	ldr	ip, .Larm9_line_size
	cmp	r1, #0x4000
	bcs	.Larm9_dcache_wbinv_all
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm9_inv_next:
	mcr	p15, 0, r0, c7, c6, 1	/* Invalidate D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm9_inv_next
	mov	pc, lr
END(arm9_dcache_inv_range)

ENTRY(arm9_idcache_wbinv_range)
	ldr	ip, .Larm9_line_size
	cmp	r1, #0x4000
	bcs	.Larm9_idcache_wbinv_all
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm9_id_wbinv_next:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c14, 1	/* Purge D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm9_id_wbinv_next
	mov	pc, lr
END(arm9_idcache_wbinv_range)

ENTRY_NP(arm9_idcache_wbinv_all)
.Larm9_idcache_wbinv_all:
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache purging code.
	 */
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */
	/* Fall through */

EENTRY(arm9_dcache_wbinv_all)
.Larm9_dcache_wbinv_all:
	ldr	ip, .Larm9_cache_data
	ldmia	ip, {s_max, i_max, s_inc, i_inc}
.Lnext_set_inv:
	orr	ip, s_max, i_max
.Lnext_index_inv:
	mcr	p15, 0, ip, c7, c14, 2	/* Purge D cache SE with Set/Index */
	subs	ip, ip, i_inc
	bhs	.Lnext_index_inv		/* Next index */
	subs	s_max, s_max, s_inc
	bhs	.Lnext_set_inv		/* Next set */
	mov	pc, lr
EEND(arm9_dcache_wbinv_all)
END(arm9_idcache_wbinv_all)

.Larm9_cache_data:
	.word	_C_LABEL(arm9_dcache_sets_max)

/*
 * Context switch.
 *
 * These is the CPU-specific parts of the context switcher cpu_switch()
 * These functions actually perform the TTB reload.
 *
 * NOTE: Special calling convention
 *	r1, r4-r13 must be preserved
 */
ENTRY(arm9_context_switch)
	/*
	 * We can assume that the caches will only contain kernel addresses
	 * at this point.  So no need to flush them again.
	 */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	mcr	p15, 0, r0, c2, c0, 0	/* set the new TTB */
	mcr	p15, 0, r0, c8, c7, 0	/* and flush the I+D tlbs */

	/* Paranoia -- make sure the pipeline is empty. */
	nop
	nop
	nop
	mov	pc, lr
END(arm9_context_switch)

	.bss

/* XXX The following macros should probably be moved to asm.h */
#define _DATA_OBJECT(x) .globl x; .type x,_ASM_TYPE_OBJECT; x:
#define C_OBJECT(x)	_DATA_OBJECT(_C_LABEL(x))

/*
 * Parameters for the cache cleaning code.  Note that the order of these
 * four variables is assumed in the code above.  Hence the reason for
 * declaring them in the assembler file.
 */
	.align 2
C_OBJECT(arm9_dcache_sets_max)
	.space	4
C_OBJECT(arm9_dcache_index_max)
	.space	4
C_OBJECT(arm9_dcache_sets_inc)
	.space	4
C_OBJECT(arm9_dcache_index_inc)
	.space	4
