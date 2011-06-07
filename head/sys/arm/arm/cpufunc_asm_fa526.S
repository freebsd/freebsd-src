/*	$NetBSD: cpufunc_asm_fa526.S,v 1.3 2008/10/15 16:56:49 matt Exp $*/
/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#ifdef CPU_FA526
#define	CACHELINE_SIZE	16
#else
#define	CACHELINE_SIZE	32
#endif

ENTRY(fa526_setttb)
	mov	r1, #0
	mcr	p15, 0, r1, c7, c14, 0	/* clean and invalidate D$ */
	mcr	p15, 0, r1, c7, c5, 0	/* invalidate I$ */
	mcr	p15, 0, r1, c7, c5, 6	/* invalidate BTB */
	mcr	p15, 0, r1, c7, c10, 4	/* drain write and fill buffer */

	mcr	p15, 0, r0, c2, c0, 0	/* Write the TTB */

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r1, c8, c7, 0	/* invalidate I+D TLB */

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
	mov	pc, lr

/*
 * TLB functions
 */
ENTRY(fa526_tlb_flushID_SE)
	mcr	p15, 0, r0, c8, c7, 1	/* flush Utlb single entry */
	mov	pc, lr

/*
 * TLB functions
 */
ENTRY(fa526_tlb_flushI_SE)
	mcr	p15, 0, r0, c8, c5, 1	/* flush Itlb single entry */
	mov	pc, lr

ENTRY(fa526_cpu_sleep)
	mov	r0, #0
/*	nop
	nop*/
	mcr	p15, 0, r0, c7, c0, 4	/* Wait for interrupt*/
	mov	pc, lr

ENTRY(fa526_flush_prefetchbuf)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 4	/* Pre-fetch flush */
	mov	pc, lr

/*
 * Cache functions
 */
ENTRY(fa526_idcache_wbinv_all)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c14, 0	/* clean and invalidate D$ */
	mcr	p15, 0, r0, c7, c5, 0	/* invalidate I$ */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(fa526_icache_sync_all)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0	/* invalidate I$ */
	mov	pc, lr

ENTRY(fa526_dcache_wbinv_all)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c14, 0	/* clean and invalidate D$ */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

/*
 * Soft functions
 */
ENTRY(fa526_dcache_wbinv_range)
	cmp	r1, #0x4000
	bhs	_C_LABEL(fa526_dcache_wbinv_all)

	and	r2, r0, #(CACHELINE_SIZE - 1)
	add	r1, r1, r2
	bic	r0, r0, #(CACHELINE_SIZE - 1)

1:	mcr	p15, 0, r0, c7, c14, 1	/* clean and invalidate D$ entry */
	add	r0, r0, #CACHELINE_SIZE
	subs	r1, r1, #CACHELINE_SIZE
	bhi	1b

	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(fa526_dcache_wb_range)
	cmp	r1, #0x4000
	bls	1f

	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 0	/* clean entire D$ */
	b	3f

1:	and	r2, r0, #(CACHELINE_SIZE - 1)
	add	r1, r1, r2
	bic	r0, r0, #(CACHELINE_SIZE - 1)

2:	mcr	p15, 0, r0, c7, c10, 1	/* clean D$ entry */
	add	r0, r0, #CACHELINE_SIZE
	subs	r1, r1, #CACHELINE_SIZE
	bhi	2b

3:	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(fa526_dcache_inv_range)
	and	r2, r0, #(CACHELINE_SIZE - 1)
	add	r1, r1, r2
	bic	r0, r0, #(CACHELINE_SIZE - 1)

1:	mcr	p15, 0, r0, c7, c6, 1	/* invalidate D$ single entry */
	add	r0, r0, #CACHELINE_SIZE
	subs	r1, r1, #CACHELINE_SIZE
	bhi	1b

	mov	pc, lr

ENTRY(fa526_idcache_wbinv_range)
	cmp	r1, #0x4000
	bhs	_C_LABEL(fa526_idcache_wbinv_all)

	and	r2, r0, #(CACHELINE_SIZE - 1)
	add	r1, r1, r2
	bic	r0, r0, #(CACHELINE_SIZE - 1)

1:	mcr	p15, 0, r0, c7, c14, 1	/* clean and invalidate D$ entry */
	mcr	p15, 0, r0, c7, c5, 1	/* invalidate I$ entry */
	add	r0, r0, #CACHELINE_SIZE
	subs	r1, r1, #CACHELINE_SIZE
	bhi	1b

2:	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(fa526_icache_sync_range)
	cmp	r1, #0x4000
	bhs	_C_LABEL(fa526_icache_sync_all)

	and	r2, r0, #(CACHELINE_SIZE - 1)
	add	r1, r1, r2
	bic	r0, r0, #(CACHELINE_SIZE - 1)

1:	mcr	p15, 0, r0, c7, c10, 1	/* clean D$ entry */
	mcr	p15, 0, r0, c7, c5, 1	/* invalidate I$ entry */
	add	r0, r0, #CACHELINE_SIZE
	subs	r1, r1, #CACHELINE_SIZE
	bhi	1b

2:	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(fa526_flush_brnchtgt_E)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 6	/* invalidate BTB cache */
	mov	pc, lr

ENTRY(fa526_context_switch)
	/*
	 * CF_CACHE_PURGE_ID will *ALWAYS* be called prior to this.
	 * Thus the data cache will contain only kernel data and the
	 * instruction cache will contain only kernel code, and all
	 * kernel mappings are shared by all processes.
	 */

	mcr	p15, 0, r0, c2, c0, 0	/* Write the TTB */

	/* If we have updated the TTB we must flush the TLB */
	mov	r0, #0
	mcr	p15, 0, r0, c8, c7, 0	/* flush the I+D tlb */

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
	mov	pc, lr

