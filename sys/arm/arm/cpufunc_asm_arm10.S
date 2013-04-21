/*	$NetBSD: cpufunc_asm_arm10.S,v 1.1 2003/09/06 09:12:29 rearnsha Exp $	*/

/*-
 * Copyright (c) 2002 ARM Limited
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
 * ARM10 assembly functions for CPU / MMU / TLB specific operations
 *
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(arm10_setttb)
	stmfd	sp!, {r0, lr}
	bl	_C_LABEL(arm10_idcache_wbinv_all)
	ldmfd	sp!, {r0, lr}

	mcr	p15, 0, r0, c2, c0, 0	/* load new TTB */

	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLBs */
	bx	lr
END(arm10_setttb)

/*
 * TLB functions
 */
ENTRY(arm10_tlb_flushID_SE)
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
	bx	lr
END(arm10_tlb_flushID_SE)

ENTRY(arm10_tlb_flushI_SE)
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
	bx	lr
END(arm10_tlb_flushI_SE)

/*
 * Cache operations.  For the entire cache we use the set/index
 * operations.
 */
	s_max	.req r0
	i_max	.req r1
	s_inc	.req r2
	i_inc	.req r3

ENTRY_NP(arm10_icache_sync_range)
	ldr	ip, .Larm10_line_size
	cmp	r1, #0x4000
	bcs	.Larm10_icache_sync_all
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm10_sync_next:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c10, 1	/* Clean D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm10_sync_next
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	bx	lr
END(arm10_icache_sync_range)

ENTRY_NP(arm10_icache_sync_all)
.Larm10_icache_sync_all:
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache cleaning code.
	 */
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */
	/* Fall through to clean Dcache. */

.Larm10_dcache_wb:
	ldr	ip, .Larm10_cache_data
	ldmia	ip, {s_max, i_max, s_inc, i_inc}
.Lnext_set:
	orr	ip, s_max, i_max
.Lnext_index:
	mcr	p15, 0, ip, c7, c10, 2	/* Clean D cache SE with Set/Index */
	subs	ip, ip, i_inc
	bhs	.Lnext_index		/* Next index */
	subs	s_max, s_max, s_inc
	bhs	.Lnext_set		/* Next set */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	bx	lr
END(arm10_icache_sync_all)

.Larm10_line_size:
	.word	_C_LABEL(arm_pdcache_line_size)

ENTRY(arm10_dcache_wb_range)
	ldr	ip, .Larm10_line_size
	cmp	r1, #0x4000
	bcs	.Larm10_dcache_wb
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm10_wb_next:
	mcr	p15, 0, r0, c7, c10, 1	/* Clean D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm10_wb_next
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	bx	lr
END(arm10_dcache_wb_range)
	
ENTRY(arm10_dcache_wbinv_range)
	ldr	ip, .Larm10_line_size
	cmp	r1, #0x4000
	bcs	.Larm10_dcache_wbinv_all
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm10_wbinv_next:
	mcr	p15, 0, r0, c7, c14, 1	/* Purge D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm10_wbinv_next
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	bx	lr
END(arm10_dcache_wbinv_range)
	
/*
 * Note, we must not invalidate everything.  If the range is too big we
 * must use wb-inv of the entire cache.
 */
ENTRY(arm10_dcache_inv_range)
	ldr	ip, .Larm10_line_size
	cmp	r1, #0x4000
	bcs	.Larm10_dcache_wbinv_all
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm10_inv_next:
	mcr	p15, 0, r0, c7, c6, 1	/* Invalidate D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm10_inv_next
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	bx	lr
END(arm10_dcache_inv_range)

ENTRY(arm10_idcache_wbinv_range)
	ldr	ip, .Larm10_line_size
	cmp	r1, #0x4000
	bcs	.Larm10_idcache_wbinv_all
	ldr	ip, [ip]
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
.Larm10_id_wbinv_next:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c14, 1	/* Purge D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bhi	.Larm10_id_wbinv_next
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	bx	lr
END(arm10_idcache_wbinv_range)

ENTRY_NP(arm10_idcache_wbinv_all)
.Larm10_idcache_wbinv_all:
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache purging code.
	 */
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */
	/* Fall through to purge Dcache. */

ENTRY(arm10_dcache_wbinv_all)
.Larm10_dcache_wbinv_all:
	ldr	ip, .Larm10_cache_data
	ldmia	ip, {s_max, i_max, s_inc, i_inc}
.Lnext_set_inv:
	orr	ip, s_max, i_max
.Lnext_index_inv:
	mcr	p15, 0, ip, c7, c14, 2	/* Purge D cache SE with Set/Index */
	subs	ip, ip, i_inc
	bhs	.Lnext_index_inv		/* Next index */
	subs	s_max, s_max, s_inc
	bhs	.Lnext_set_inv		/* Next set */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	bx	lr
END(arm10_idcache_wbinv_all)
END(arm10_dcache_wbinv_all)

.Larm10_cache_data:
	.word	_C_LABEL(arm10_dcache_sets_max)

/*
 * Context switch.
 *
 * These is the CPU-specific parts of the context switcher cpu_switch()
 * These functions actually perform the TTB reload.
 *
 * NOTE: Special calling convention
 *	r1, r4-r13 must be preserved
 */
ENTRY(arm10_context_switch)
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
	bx	lr
END(arm10_context_switch)

	.bss

/* XXX The following macros should probably be moved to asm.h */
#define _DATA_OBJECT(x) .globl x; .type x,_ASM_TYPE_OBJECT; x:
#define C_OBJECT(x)	_DATA_OBJECT(_C_LABEL(x))

/*
 * Parameters for the cache cleaning code.  Note that the order of these
 * four variables is assumed in the code above.  Hence the reason for
 * declaring them in the assembler file.
 */
	.align 0
C_OBJECT(arm10_dcache_sets_max)
	.space	4
C_OBJECT(arm10_dcache_index_max)
	.space	4
C_OBJECT(arm10_dcache_sets_inc)
	.space	4
C_OBJECT(arm10_dcache_index_inc)
	.space	4
