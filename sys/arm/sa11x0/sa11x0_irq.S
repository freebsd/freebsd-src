/*	$NetBSD: sa11x0_irq.S,v 1.5 2003/03/31 19:52:35 chris Exp $	*/

/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Mark Brinicombe
 *      for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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
Lcurrent_spl_level:	
	.word	_C_LABEL(current_spl_level)
Lcurrent_intr_depth:
	.word	_C_LABEL(current_intr_depth)

Lspl_masks:
	.word	_C_LABEL(spl_masks)
	.globl	_C_LABEL(saipic_base)
_C_LABEL(saipic_base):
	.word	0x00000000

#ifdef INTR_DEBUG
Ldbg_str:
	.asciz	"irq_entry %x %x\n"
#endif

AST_ALIGNMENT_FAULT_LOCALS
/*
 * Regsister usage
 *
 *  r6  - Address of current handler
 *  r7  - Pointer to handler pointer list
 *  r8  - Current IRQ requests.
 *  r9  - Used to count through possible IRQ bits.
 *  r10 - Base address of SAIP
 */

#define _SPL_LEVELS	13 /* XXX */
ASENTRY_NP(irq_entry)
	sub	lr, lr, #0x00000004	/* Adjust the lr */

	PUSHFRAMEINSVC			/* Push an interrupt frame */
	ENABLE_ALIGNMENT_FAULTS

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

	/*
 	 * Need to block all interrupts at the IPL or lower for
	 * all asserted interrupts.
	 * This basically emulates hardware interrupt priority levels.
	 * Means we need to go through the interrupt mask and for
	 * every asserted interrupt we need to mask out all other
	 * interrupts at the same or lower IPL.
	 * If only we could wait until the main loop but we need to sort
	 * this out first so interrupts can be re-enabled.
	 *
	 * This would benefit from a special ffs type routine
	 */

	mov	r9, #(_SPL_LEVELS - 1)
	ldr	r7, Lspl_masks
Lfind_highest_ipl:
	ldr	r2, [r7, r9, lsl #2]
	tst	r8, r2
	subeq	r9, r9, #1
	beq	Lfind_highest_ipl

	/* r9 = SPL level of highest priority interrupt */
	add	r9, r9, #1
	ldr	r2, [r7, r9, lsl #2]
	mvn	r2, r2

	ldr	r0, Lcurrent_spl_level
	ldr	r1, [r0]
	str	r9, [r0]
	stmfd	sp!, {r1}

	/* Update the SAIP irq masks */
	bl	_C_LABEL(irq_setmasks)
#ifdef INTR_DEBUG
	stmfd	sp!, {r0,r1,r2}
	adr	r0, Ldbg_str
	mov	r2, r9
	bl	_C_LABEL(printf)
	ldmia	sp!, {r0,r1,r2}
#endif
        mrs     r0, cpsr_all		/* Enable IRQ's */
	bic	r0, r0, #I32_bit
	msr	cpsr_all, r0
	mov	r0, r8
	bl	_C_LABEL(arm_handler_execute)

	ldmfd	sp!, {r2}
	ldr	r1, Lcurrent_spl_level
	str	r2, [r1]
	/* Restore previous disabled mask */
	bl	_C_LABEL(irq_setmasks)
	bl	_C_LABEL(dosoftints)	/* Handle the soft interrupts */

	/* Kill IRQ's in preparation for exit */
        mrs     r0, cpsr_all
        orr     r0, r0, #(I32_bit)
        msr     cpsr_all, r0

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

	DO_AST_AND_RESTORE_ALIGNMENT_FAULTS
	PULLFRAMEFROMSVCANDEXIT

	/* NOT REACHED */
	b	. - 8
ENTRY(irq_setmasks)
	/* Disable interrupts */
	mrs	r3, cpsr_all
	orr	r1, r3,  #(I32_bit)
	msr	cpsr_all, r1

	/* Calculate interrupt mask */
	ldr	r0, Lspl_masks
	ldr	r2, Lcurrent_spl_level
	ldr	r2, [r2]
	ldr	r2, [r0, r2, lsl #2]
	ldr	r0, _C_LABEL(saipic_base)
	str	r2, [r0, #(SAIPIC_MR)]	/* Set mask register */

	/* Restore old cpsr and exit */
	/* msr	cpsr_all, r3 XXX: not now.*/
	mov	pc, lr
Lcnt:
	.word	_C_LABEL(cnt)

#ifdef IRQSTATS
Lintrcnt:
	.word	_C_LABEL(intrcnt)
#endif

Lirqhandlers:
	.word	_C_LABEL(irqhandlers)	/* Pointer to array of irqhandlers */




	.global _C_LABEL(intrnames), _C_LABEL(eintrnames)
	.global _C_LABEL(eintrcnt)
_C_LABEL(intrnames): 
_C_LABEL(eintrnames):
_C_LABEL(eintrcnt):

	.globl	_C_LABEL(intrcnt), _C_LABEL(sintrcnt)

_C_LABEL(intrcnt):
	.space	ICU_LEN*4  /* XXX Should be linked to number of interrupts */

_C_LABEL(sintrcnt):
	.space 32*4
