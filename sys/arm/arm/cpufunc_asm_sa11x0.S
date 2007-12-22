/*	$NetBSD: cpufunc_asm_sa11x0.S,v 1.3 2002/08/17 16:36:32 thorpej Exp $	*/

/*-
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

	.data
	.global	_C_LABEL(sa11x0_idle_mem)
_C_LABEL(sa11x0_idle_mem):
	.word	0

	.text

	.align	5

	/* We're now 32-byte aligned */

.Lsa11x0_idle_mem:
	.word	_C_LABEL(sa11x0_idle_mem)				/* 1 */

/*
 * sa11x0_cpusleep
 *
 * This is called when there is nothing on any of the run queues.
 * We go into IDLE mode so that any IRQ or FIQ will awaken us.
 */
ENTRY(sa11x0_cpu_sleep)
	ldr	r1, .Lsa11x0_idle_mem	/* get address of... */		/* 2 */
	nop								/* 3 */
	ldr	r1, [r1]		/* ...non-cacheable page */	/* 4 */
	nop								/* 5 */

	/*
	 * SA-1110 manual, 9.5.2.1 (Entering Idle Mode) says that
	 * to enter idle mode:
	 *
	 *	* Disable clock switching
	 *	* Issue load from non-cacheable address
	 *	* Issue "wait for interrupt"
	 *
	 * The 3-insn sequence must reside in the first 3 words
	 * of a cache line.
	 *
	 * We must disable interrupts in the CPSR so that we can
	 * re-enable clock switching before servicing interrupts.
	 */

	mrs	r3, cpsr_all						/* 6 */
	orr	r2, r3, #(I32_bit|F32_bit)				/* 7 */
	msr	cpsr_all, r2						/* 8 */

	/* We're now 32-byte aligned */

	mcr	p15, 0, r0, c15, c2, 2	/* disable clock switching */
	ldr	r0, [r1]		/* load from non-cacheable address */
	mcr	p15, 0, r0, c15, c8, 2	/* wait for interrupt */

	mcr	p15, 0, r0, c15, c1, 2	/* re-enable clock switching */

	/* Restore interrupts (which will cause them to be serviced). */
	msr	cpsr_all, r3
	RET


/*
 * This function is the same as sa110_context_switch for now, the plan
 * is to make use of the process id register to avoid cache flushes.
 */
ENTRY(sa11x0_context_switch)
	/*
	 * CF_CACHE_PURGE_ID will *ALWAYS* be called prior to this.
	 * Thus the data cache will contain only kernel data and the
	 * instruction cache will contain only kernel code, and all
	 * kernel mappings are shared by all processes.
	 */

	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c8, c7, 0	/* flush the I+D tlb */

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
	RET

ENTRY(sa11x0_drain_readbuf)
	mcr	p15, 0, r0, c9, c0, 0		/* drain read buffer */
	RET
