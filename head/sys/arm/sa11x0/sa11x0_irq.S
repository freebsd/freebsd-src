/*	$NetBSD: sa11x0_irq.S,v 1.5 2003/03/31 19:52:35 chris Exp $	*/

/*-
 * Copyright (c) 1998 Mark Brinicombe.
 * Copyright (c) 1998 Causality Limited
 * All rights reserved.
 *
 * This code is derived from software contributed to the NetBSD Foundation
 * by IWAMOTO Toshihiro.
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
 */

#include "assym.s"
#include <machine/asm.h>
#include <machine/armreg.h>
#include <machine/asmacros.h>
#include <arm/sa11x0/sa11x0_reg.h>
__FBSDID("$FreeBSD$");
Lcurrent_intr_depth:
	.word	_C_LABEL(current_intr_depth)

	.globl	_C_LABEL(saipic_base)
_C_LABEL(saipic_base):
	.word	0x00000000

#ifdef INTR_DEBUG
Ldbg_str:
	.asciz	"irq_entry %x %x\n"
#endif

AST_LOCALS
/*
 * Regsister usage
 *
 *  r6  - Address of current handler
 *  r7  - Pointer to handler pointer list
 *  r8  - Current IRQ requests.
 *  r9  - Used to count through possible IRQ bits.
 *  r10 - Base address of SAIP
 */

ASENTRY_NP(irq_entry)
	sub	lr, lr, #0x00000004	/* Adjust the lr */

	PUSHFRAMEINSVC			/* Push an interrupt frame */

	/* Load r8 with the SAIPIC interrupt requests */

	ldr	r10, _C_LABEL(saipic_base)
	ldr	r8, [r10, #(SAIPIC_IP)]	/* Load IRQ pending register */

#ifdef INTR_DEBUG
	ldr	r2, [r10, #(SAIPIC_MR)]
	adr	r0, Ldbg_str
	mov	r1, r8
	bl	_C_LABEL(printf)
#endif
	/*
	 * Note that we have entered the IRQ handler.
	 * We are in SVC mode so we cannot use the processor mode
	 * to determine if we are in an IRQ. Instead we will count the
	 * each time the interrupt handler is nested.
	 */

	ldr	r0, Lcurrent_intr_depth
	ldr	r1, [r0]
	add	r1, r1, #1
	str	r1, [r0]

	mov	r0, sp
	mov	r1, r8
	bl	_C_LABEL(arm_handler_execute)

#ifdef INTR_DEBUG
	adr	r0, Ldbg_str
	mov	r1, #3
	ldr	r2, [r10, #(SAIPIC_MR)]
	bl	_C_LABEL(printf)
#endif

	/* Decrement the nest count */
	ldr	r0, Lcurrent_intr_depth
	ldr	r1, [r0]
	sub	r1, r1, #1
	str	r1, [r0]

	DO_AST
	PULLFRAMEFROMSVCANDEXIT

	/* NOT REACHED */
	b	. - 8
Lcnt:
	.word	_C_LABEL(cnt)

ENTRY(sa11x0_activateirqs)
	ldr	r0, _C_LABEL(saipic_base)
	mov	r1, #0xffffffff
	str	r1, [r0, #(SAIPIC_MR)]
	mov	pc, lr

	.global _C_LABEL(intrnames), _C_LABEL(sintrnames)
_C_LABEL(intrnames):
_C_LABEL(sintrnames):
	.int	0

	.globl	_C_LABEL(intrcnt), _C_LABEL(sintrcnt)

_C_LABEL(intrcnt):
	.space	ICU_LEN*4  /* XXX Should be linked to number of interrupts */

_C_LABEL(sintrcnt):
	.word	ICU_LEN*4
