/*-
 * Copyright (c) 2014 Robin Randhawa
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation
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
 */

#include <sys/elf_common.h>
#include <sys/errno.h>

#include <machine/asm.h>
#include <machine/param.h>

#include "assym.inc"
/*
 * FIXME:
 * Need big.LITTLE awareness at some point.
 * Using arm64_p[id]cache_line_size may not be the best option.
 * Need better SMP awareness.
 */
	.text
	.align	2

.Lpage_mask:
	.word	PAGE_MASK

/*
 * Macro to apply "insn op" to each cache line that overlaps the address
 * range ["addr", "addr" + "size"), where "size" is non-zero.  This macro
 * expects the cache line size to be in x3 and "addr" to be aligned to that
 * size.  It will corrupt "addr" and "size".
 */
.macro apply_to_range insn, op, addr, size
1:
	\insn	\op, \addr
	add	\addr, \addr, x3	/* Move to the next line */
	subs	\size, \size, x3	/* Reduce the size */
	b.hi	1b			/* Check if we are done */
.endm

/*
 * Macro to handle the cache. This takes the start address in x0, length
 * in x1. It will corrupt x0, x1, x2, x3, and x4.
 */
.macro cache_handle_range dc = 1, dcop = 0, ic = 0, icop = 0
.if \ic == 0
	adrp	x3, dcache_line_size	/* Load the D cache line size */
	ldr	x3, [x3, :lo12:dcache_line_size]
.elseif \dc == 0
	adrp	x3, icache_line_size	/* Load the I cache line size */
	ldr	x3, [x3, :lo12:icache_line_size]
.else
	adrp	x3, idcache_line_size	/* Load the I & D cache line size */
	ldr	x3, [x3, :lo12:idcache_line_size]
.endif
	sub	x4, x3, #1		/* Get the address mask */
	and	x2, x0, x4		/* Get the low bits of the address */
	add	x1, x1, x2		/* Add these to the size */
	bic	x0, x0, x4		/* Clear the low bit of the address */
.if \dc != 0
.if \ic != 0
	mov	x2, x0			/* Save the address */
	mov	x4, x1			/* Save the size */
.endif
	apply_to_range	dc, \dcop, x0, x1
	dsb	ish
.endif
.if \ic != 0
.if \dc != 0
	apply_to_range	ic, \icop, x2, x4
.else
	apply_to_range	ic, \icop, x0, x1
.endif
	dsb	ish
	isb
.endif
.endm

ENTRY(arm64_nullop)
	ret
END(arm64_nullop)

/*
 * void arm64_dcache_wb_range(void *, vm_size_t)
 */
ENTRY(arm64_dcache_wb_range)
	cache_handle_range	dcop = cvac
	ret
END(arm64_dcache_wb_range)

/*
 * void arm64_dcache_wbinv_range(void *, vm_size_t)
 */
ENTRY(arm64_dcache_wbinv_range)
	cache_handle_range	dcop = civac
	ret
END(arm64_dcache_wbinv_range)

/*
 * void arm64_dcache_inv_range(void *, vm_size_t)
 *
 * Note, we must not invalidate everything.  If the range is too big we
 * must use wb-inv of the entire cache.
 */
ENTRY(arm64_dcache_inv_range)
	cache_handle_range	dcop = ivac
	ret
END(arm64_dcache_inv_range)

/*
 * void arm64_dic_idc_icache_sync_range(void *, vm_size_t)
 * When the CTR_EL0.IDC bit is set cleaning to PoU becomes a dsb.
 * When the CTR_EL0.DIC bit is set icache invalidation becomes an isb.
 */
ENTRY(arm64_dic_idc_icache_sync_range)
	dsb	ishst
	isb
	ret
END(arm64_dic_idc_icache_sync_range)

/*
 * void arm64_idc_aliasing_icache_sync_range(void *, vm_size_t)
 * When the CTR_EL0.IDC bit is set cleaning to PoU becomes a dsb.
 */
ENTRY(arm64_idc_aliasing_icache_sync_range)
	dsb	ishst
	ic	ialluis
	dsb	ish
	isb
	ret
END(arm64_idc_aliasing_icache_sync_range)

/*
 * Invalidating a PIPT icache by virtual address broadcasts an invalidation
 * for each line to the other CPUs.  For a range larger than 32KB, such as
 * a superpage, invalidating the entire icache is likely to be cheaper.
 */
#define	ICACHE_SYNC_RANGE_MAX	(32 * 1024)

/*
 * void arm64_idc_pipt_icache_sync_range(void *, vm_size_t)
 *
 * When the CTR_EL0.IDC bit is set cleaning to PoU becomes a dsb.
 * A PIPT icache can be invalidated by virtual address, but a large range
 * is better handled by arm64_idc_aliasing_icache_sync_range()'s ialluis.
 */
ENTRY(arm64_idc_pipt_icache_sync_range)
	cmp	x1, #ICACHE_SYNC_RANGE_MAX
	b.hi	arm64_idc_aliasing_icache_sync_range
	dsb	ishst
	cache_handle_range	dc = 0, ic = 1, icop = ivau
	ret
END(arm64_idc_pipt_icache_sync_range)

/*
 * void arm64_aliasing_icache_sync_range(void *, vm_size_t)
 *
 * Invalidating an aliasing icache by virtual address could miss lines that
 * were fetched through a different mapping, so invalidate the entire icache.
 */
ENTRY(arm64_aliasing_icache_sync_range)
	cache_handle_range	dcop = cvau
	ic	ialluis
	dsb	ish
	isb
	ret
END(arm64_aliasing_icache_sync_range)

/*
 * void arm64_pipt_icache_sync_range(void *, vm_size_t)
 *
 * A PIPT icache can be invalidated by virtual address, but a large range
 * is better handled by arm64_aliasing_icache_sync_range()'s ialluis.
 */
ENTRY(arm64_pipt_icache_sync_range)
	cmp	x1, #ICACHE_SYNC_RANGE_MAX
	b.hi	arm64_aliasing_icache_sync_range
	cache_handle_range	dcop = cvau, ic = 1, icop = ivau
	ret
END(arm64_pipt_icache_sync_range)

/*
 * int arm64_icache_sync_range_checked(void *, vm_size_t)
 */
ENTRY(arm64_icache_sync_range_checked)
	adr	x5, cache_maint_fault
	SET_FAULT_HANDLER(x5, x6)
	cache_handle_range	dcop = cvau
	ic	ialluis
	dsb	ish
	isb
	SET_FAULT_HANDLER(xzr, x6)
	mov	x0, #0
	ret
END(arm64_icache_sync_range_checked)

ENTRY(cache_maint_fault)
	SET_FAULT_HANDLER(xzr, x1)
	mov	x0, #EFAULT
	ret
END(cache_maint_fault)

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
