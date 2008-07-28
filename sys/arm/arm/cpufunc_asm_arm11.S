/*	$NetBSD: cpufunc_asm_arm11.S,v 1.2 2005/12/11 12:16:41 christos Exp $	*/

/*
 * Copyright (c) 2002, 2005 ARM Limited
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
 * ARM11 assembly functions for CPU / MMU / TLB specific operations
 *
 * XXX We make no attempt at present to take advantage of the v6 memroy
 * architecture or physically tagged cache.
 */
 
#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(arm11_setttb)
	stmfd	sp!, {r0, lr}
	bl	_C_LABEL(armv5_idcache_wbinv_all)
	ldmfd	sp!, {r0, lr}

	mcr	p15, 0, r0, c2, c0, 0	/* load new TTB */

	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLBs */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET

/*
 * TLB functions
 */
ENTRY(arm11_tlb_flushID_SE)
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET

ENTRY(arm11_tlb_flushI_SE)
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
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
ENTRY(arm11_context_switch)
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
	RET

/*
 * TLB functions
 */
ENTRY(arm11_tlb_flushID)
	mcr	p15, 0, r0, c8, c7, 0	/* flush I+D tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(arm11_tlb_flushI)
	mcr	p15, 0, r0, c8, c5, 0	/* flush I tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(arm11_tlb_flushD)
	mcr	p15, 0, r0, c8, c6, 0	/* flush D tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(arm11_tlb_flushD_SE)
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

/*
 * Other functions
 */
ENTRY(arm11_drain_writebuf)
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr
