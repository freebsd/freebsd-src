/*	$NetBSD: cpufunc_asm_armv6.S,v 1.4 2010/12/10 02:06:22 bsh Exp $	*/

/*
 * Copyright (c) 2002, 2005 ARM Limited
 * Portions Copyright (c) 2007 Microsoft
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
 * ARMv6 assembly functions for manipulating caches.
 * These routines can be used by any core that supports the mcrr address
 * range operations.
 */

/*
 * $FreeBSD$
 */
 
#include <machine/asm.h>

	.arch	armv6

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(armv6_setttb)
#ifdef PMAP_CACHE_VIVT
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */
	mcr	p15, 0, r0, c7, c14, 0	/* clean and invalidate D cache */
#endif
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */

	mcr	p15, 0, r0, c2, c0, 0	/* load new TTB */

	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLBs */
	RET
END(armv6_setttb)

/*
 * Cache operations.
 */

/* LINTSTUB: void armv6_icache_sync_range(vaddr_t, vsize_t); */
ENTRY_NP(armv6_icache_sync_range)
	add	r1, r1, r0
	sub	r1, r1, #1
	mcrr	p15, 0, r1, r0, c5	/* invalidate I cache range */
	mcrr	p15, 0, r1, r0, c12	/* clean D cache range */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	RET
END(armv6_icache_sync_range)

/* LINTSTUB: void armv6_icache_sync_all(void); */
ENTRY_NP(armv6_icache_sync_all)
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache cleaning code.
	 */
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */
	mcr	p15, 0, r0, c7, c10, 0	/* Clean D cache */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	RET
END(armv6_icache_sync_all)

/* LINTSTUB: void armv6_dcache_wb_range(vaddr_t, vsize_t); */
ENTRY(armv6_dcache_wb_range)
	add	r1, r1, r0
	sub	r1, r1, #1
	mcrr	p15, 0, r1, r0, c12	/* clean D cache range */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	RET
END(armv6_dcache_wb_range)
	
/* LINTSTUB: void armv6_dcache_wbinv_range(vaddr_t, vsize_t); */
ENTRY(armv6_dcache_wbinv_range)
	add	r1, r1, r0
	sub	r1, r1, #1
	mcrr	p15, 0, r1, r0, c14	/* clean and invaliate D cache range */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	RET
END(armv6_dcache_wbinv_range)
	
/*
 * Note, we must not invalidate everything.  If the range is too big we
 * must use wb-inv of the entire cache.
 *
 * LINTSTUB: void armv6_dcache_inv_range(vaddr_t, vsize_t);
 */
ENTRY(armv6_dcache_inv_range)
	add	r1, r1, r0
	sub	r1, r1, #1
	mcrr	p15, 0, r1, r0, c6	/* invaliate D cache range */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	RET
END(armv6_dcache_inv_range)

/* LINTSTUB: void armv6_idcache_wbinv_range(vaddr_t, vsize_t); */
ENTRY(armv6_idcache_wbinv_range)
	add	r1, r1, r0
	sub	r1, r1, #1
	mcrr	p15, 0, r1, r0, c5	/* invaliate I cache range */
	mcrr	p15, 0, r1, r0, c14	/* clean & invaliate D cache range */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	RET
END(armv6_idcache_wbinv_range)

/* LINTSTUB: void armv6_idcache_wbinv_all(void); */
ENTRY_NP(armv6_idcache_wbinv_all)
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache purging code.
	 */
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */
	/* Fall through to purge Dcache. */

/* LINTSTUB: void armv6_dcache_wbinv_all(void); */
ENTRY(armv6_dcache_wbinv_all)
	mcr	p15, 0, r0, c7, c14, 0	/* clean & invalidate D cache */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	RET
END(armv6_idcache_wbinv_all)
END(armv6_dcache_wbinv_all)

