/*	$NetBSD: cpufunc_asm_arm8.S,v 1.2 2001/11/11 00:47:49 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 ARM Limited
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
 * ARM8 assembly functions for CPU / MMU / TLB specific operations
 *
 */
 
#include <machine/asm.h>
__FBSDID("$FreeBSD: src/sys/arm/arm/cpufunc_asm_arm8.S,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $");

ENTRY(arm8_clock_config)
	mrc	p15, 0, r3, c15, c0, 0	/* Read the clock register */
	bic	r2, r3, #0x11		/* turn off dynamic clocking
					   and clear L bit */
	mcr	p15, 0, r2, c15, c0, 0	/* Write clock register */

	bic	r2, r3, r0		/* Clear bits */
	eor	r2, r2, r1		/* XOR bits */
	bic	r2, r2, #0x10		/* clear the L bit */

	bic	r1, r2, #0x01		/* still keep dynamic clocking off */
	mcr	p15, 0, r1, c15, c0, 0	/* Write clock register */
	mov	r0, r0			/* NOP */
	mov	r0, r0			/* NOP */
	mov	r0, r0			/* NOP */
	mov	r0, r0			/* NOP */
	mcr	p15, 0, r2, c15, c0, 0 	/* Write clock register */
	mov	r0, r3			/* Return old value */
	RET

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(arm8_setttb)
	mrs	r3, cpsr_all
	orr	r1, r3, #(I32_bit | F32_bit)
	msr	cpsr_all, r1

	stmfd	sp!, {r0-r3, lr}
	bl	_C_LABEL(arm8_cache_cleanID)
	ldmfd	sp!, {r0-r3, lr}
	mcr	p15, 0, r0, c7, c7, 0	/* flush I+D cache */

	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c8, c7, 0

	/* For good measure we will flush the IDC as well */
	mcr	p15, 0, r0, c7, c7, 0

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
	msr	cpsr_all, r3

	RET

/*
 * TLB functions
 */
ENTRY(arm8_tlb_flushID)
	mcr	p15, 0, r0, c8, c7, 0	/* flush I+D tlb */
	RET

ENTRY(arm8_tlb_flushID_SE)
	mcr	p15, 0, r0, c8, c7, 1	/* flush I+D tlb single entry */
	RET

/*
 * Cache functions
 */
ENTRY(arm8_cache_flushID)
	mcr	p15, 0, r0, c7, c7, 0	/* flush I+D cache */
	RET

ENTRY(arm8_cache_flushID_E)
	mcr	p15, 0, r0, c7, c7, 1	/* flush I+D single entry */
	RET

ENTRY(arm8_cache_cleanID)
	mov	r0, #0x00000000

1:	mov	r2, r0
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1

	adds	r0, r0, #0x04000000
	bne	1b

	RET

ENTRY(arm8_cache_cleanID_E)
	mcr	p15, 0, r0, c7, c11, 1	/* clean I+D single entry */
	RET

ENTRY(arm8_cache_purgeID)
	/*
	 * ARM810 bug 3
	 *
	 * Clean and invalidate entry will not invalidate the entry
	 * if the line was already clean. (mcr p15, 0, rd, c7, 15, 1)
	 *
	 * Instead of using the clean and invalidate entry operation
	 * use a separate clean and invalidate entry operations.
	 * i.e.
	 * mcr p15, 0, rd, c7, c11, 1
	 * mcr p15, 0, rd, c7, c7, 1
	 */

	mov	r0, #0x00000000

	mrs	r3, cpsr_all
	orr	r2, r3, #(I32_bit | F32_bit)
	msr	cpsr_all, r2

1:	mov	r2, r0
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1
	add	r2, r2, #0x10
	mcr	p15, 0, r2, c7, c11, 1
	mcr	p15, 0, r2, c7, c7, 1

	adds	r0, r0, #0x04000000
	bne	1b

	msr	cpsr_all, r3
	RET

ENTRY(arm8_cache_purgeID_E)
	/*
	 * ARM810 bug 3
	 *
	 * Clean and invalidate entry will not invalidate the entry
	 * if the line was already clean. (mcr p15, 0, rd, c7, 15, 1)
	 *
	 * Instead of using the clean and invalidate entry operation
	 * use a separate clean and invalidate entry operations.
	 * i.e.
	 * mcr p15, 0, rd, c7, c11, 1
	 * mcr p15, 0, rd, c7, c7, 1
	 */
	mrs	r3, cpsr_all
	orr	r2, r3, #(I32_bit | F32_bit)
	msr	cpsr_all, r2
	mcr	p15, 0, r0, c7, c11, 1	/* clean I+D single entry */
	mcr	p15, 0, r0, c7, c7, 1	/* flush I+D single entry */
	msr	cpsr_all, r3
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
ENTRY(arm8_context_switch)
	/* For good measure we will flush the IDC as well */
	mcr	p15, 0, r0, c7, c7, 0	/* flush I+D cache */

	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c8, c7, 0	/* flush the I+D tlb */

#if 0
	/* For good measure we will flush the IDC as well */
	mcr	p15, 0, r0, c7, c7, 0	/* flush I+D cache */
#endif

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
	RET
