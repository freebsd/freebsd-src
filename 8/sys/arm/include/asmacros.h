/*	$NetBSD: frame.h,v 1.6 2003/10/05 19:44:58 matt Exp $	*/

/*-
 * Copyright (c) 1994-1997 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_ASMACROS_H_
#define	_MACHINE_ASMACROS_H_

#ifdef _KERNEL

#ifdef LOCORE

/*
 * ASM macros for pushing and pulling trapframes from the stack
 *
 * These macros are used to handle the irqframe and trapframe structures
 * defined above.
 */

/*
 * PUSHFRAME - macro to push a trap frame on the stack in the current mode
 * Since the current mode is used, the SVC lr field is not defined.
 *
 * NOTE: r13 and r14 are stored separately as a work around for the
 * SA110 rev 2 STM^ bug
 */

#define PUSHFRAME							   \
	str	lr, [sp, #-4]!;		/* Push the return address */	   \
	sub	sp, sp, #(4*17);	/* Adjust the stack pointer */	   \
	stmia	sp, {r0-r12};		/* Push the user mode registers */ \
	add	r0, sp, #(4*13);	/* Adjust the stack pointer */	   \
	stmia	r0, {r13-r14}^;		/* Push the user mode registers */ \
        mov     r0, r0;                 /* NOP for previous instruction */ \
	mrs	r0, spsr_all;		/* Put the SPSR on the stack */	   \
	str	r0, [sp, #-4]!;						   \
	ldr	r0, =ARM_RAS_START;					   \
	mov	r1, #0;							   \
	str	r1, [r0];						   \
	ldr	r0, =ARM_RAS_END;					   \
	mov	r1, #0xffffffff;					   \
	str	r1, [r0];

/*
 * PULLFRAME - macro to pull a trap frame from the stack in the current mode
 * Since the current mode is used, the SVC lr field is ignored.
 */

#define PULLFRAME							   \
        ldr     r0, [sp], #0x0004;      /* Get the SPSR from stack */	   \
        msr     spsr_all, r0;						   \
        ldmia   sp, {r0-r14}^;		/* Restore registers (usr mode) */ \
        mov     r0, r0;                 /* NOP for previous instruction */ \
	add	sp, sp, #(4*17);	/* Adjust the stack pointer */	   \
 	ldr	lr, [sp], #0x0004;	/* Pull the return address */

/*
 * PUSHFRAMEINSVC - macro to push a trap frame on the stack in SVC32 mode
 * This should only be used if the processor is not currently in SVC32
 * mode. The processor mode is switched to SVC mode and the trap frame is
 * stored. The SVC lr field is used to store the previous value of
 * lr in SVC mode.  
 *
 * NOTE: r13 and r14 are stored separately as a work around for the
 * SA110 rev 2 STM^ bug
 */

#define PUSHFRAMEINSVC							   \
	stmdb	sp, {r0-r3};		/* Save 4 registers */		   \
	mov	r0, lr;			/* Save xxx32 r14 */		   \
	mov	r1, sp;			/* Save xxx32 sp */		   \
	mrs	r3, spsr;		/* Save xxx32 spsr */		   \
	mrs     r2, cpsr; 		/* Get the CPSR */		   \
	bic     r2, r2, #(PSR_MODE);	/* Fix for SVC mode */		   \
	orr     r2, r2, #(PSR_SVC32_MODE);				   \
	msr     cpsr_c, r2;		/* Punch into SVC mode */	   \
	mov	r2, sp;			/* Save	SVC sp */		   \
	str	r0, [sp, #-4]!;		/* Push return address */	   \
	str	lr, [sp, #-4]!;		/* Push SVC lr */		   \
	str	r2, [sp, #-4]!;		/* Push SVC sp */		   \
	msr     spsr_all, r3;		/* Restore correct spsr */	   \
	ldmdb	r1, {r0-r3};		/* Restore 4 regs from xxx mode */ \
	sub	sp, sp, #(4*15);	/* Adjust the stack pointer */	   \
	stmia	sp, {r0-r12};		/* Push the user mode registers */ \
	add	r0, sp, #(4*13);	/* Adjust the stack pointer */	   \
	stmia	r0, {r13-r14}^;		/* Push the user mode registers */ \
        mov     r0, r0;                 /* NOP for previous instruction */ \
	ldr	r5, =ARM_RAS_START;	/* Check if there's any RAS */	   \
	ldr	r3, [r5];						   \
	cmp	r3, #0;			/* Is the update needed ? */	   \
	ldrgt	lr, [r0, #16];						   \
	ldrgt	r1, =ARM_RAS_END;					   \
	ldrgt	r4, [r1];		/* Get the end of the RAS */	   \
	movgt	r2, #0;			/* Reset the magic addresses */	   \
	strgt	r2, [r5];						   \
	movgt	r2, #0xffffffff;					   \
	strgt	r2, [r1];						   \
	cmpgt	lr, r3;			/* Were we in the RAS ? */	   \
	cmpgt	r4, lr;							   \
	strgt	r3, [r0, #16];		/* Yes, update the pc */	   \
	mrs	r0, spsr_all;		/* Put the SPSR on the stack */	   \
	str	r0, [sp, #-4]!

/*
 * PULLFRAMEFROMSVCANDEXIT - macro to pull a trap frame from the stack
 * in SVC32 mode and restore the saved processor mode and PC.
 * This should be used when the SVC lr register needs to be restored on
 * exit.
 */

#define PULLFRAMEFROMSVCANDEXIT						   \
        ldr     r0, [sp], #0x0004;	/* Get the SPSR from stack */	   \
        msr     spsr_all, r0;		/* restore SPSR */		   \
        ldmia   sp, {r0-r14}^;		/* Restore registers (usr mode) */ \
        mov     r0, r0;	  		/* NOP for previous instruction */ \
	add	sp, sp, #(4*15);	/* Adjust the stack pointer */	   \
	ldmia	sp, {sp, lr, pc}^	/* Restore lr and exit */

#define	DATA(name) \
	.data ; \
	_ALIGN_DATA ; \
	.globl	name ; \
	.type	name, %object ; \
name:

#define	EMPTY

		
#define	DO_AST								\
	ldr	r0, [sp]		/* Get the SPSR from stack */	;\
	mrs	r4, cpsr		/* save CPSR */			;\
	orr	r1, r4, #(I32_bit|F32_bit)				;\
	msr	cpsr_c, r1		/* Disable interrupts */	;\
	and	r0, r0, #(PSR_MODE)	/* Returning to USR mode? */	;\
	teq	r0, #(PSR_USR32_MODE)					;\
	bne	2f			/* Nope, get out now */		;\
	bic	r4, r4, #(I32_bit|F32_bit)				;\
1:	ldr	r5, .Lcurthread						;\
	ldr	r5, [r5]						;\
	ldr	r1, [r5, #(TD_FLAGS)]					;\
	and	r1, r1, #(TDF_ASTPENDING|TDF_NEEDRESCHED)		;\
	teq	r1, #0x00000000						;\
	beq	2f			/* Nope. Just bail */		;\
	msr	cpsr_c, r4		/* Restore interrupts */	;\
	mov	r0, sp							;\
	bl	_C_LABEL(ast)		/* ast(frame) */		;\
	orr	r0, r4, #(I32_bit|F32_bit)				;\
	msr	cpsr_c, r0						;\
	b	1b							;\
2:


#define	AST_LOCALS							;\
.Lcurthread:								;\
	.word	_C_LABEL(__pcpu) + PC_CURTHREAD

#endif /* LOCORE */

#endif /* _KERNEL */

#endif /* !_MACHINE_ASMACROS_H_ */
