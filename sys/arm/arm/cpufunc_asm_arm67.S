/*	$NetBSD: cpufunc_asm_arm67.S,v 1.1 2001/11/10 23:14:09 thorpej Exp $	*/

/*
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
 * ARM6/ARM7 assembly functions for CPU / MMU / TLB specific operations
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
ENTRY(arm67_setttb)
	mcr	p15, 0, r0, c7, c0, 0

	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c5, c0, 0

	/* For good measure we will flush the IDC as well */
	mcr	p15, 0, r0, c7, c0, 0

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0

	RET

/*
 * TLB functions
 */
ENTRY(arm67_tlb_flush)
	mcr	p15, 0, r0, c5, c0, 0 
	RET

ENTRY(arm67_tlb_purge)
	mcr	p15, 0, r0, c6, c0, 0
	RET

/*
 * Cache functions
 */
ENTRY(arm67_cache_flush)
	mcr	p15, 0, r0, c7, c0, 0
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
ENTRY(arm67_context_switch)
	/* For good measure we will flush the IDC as well */
	mcr	p15, 0, r0, c7, c0, 0	/* flush cache */

	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c5, c0, 0

#if 0
	/* For good measure we will flush the IDC as well */
	mcr	p15, 0, r0, c7, c0, 0	/* flush cache */
#endif

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
	RET
