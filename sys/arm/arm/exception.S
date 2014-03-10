/*	$NetBSD: exception.S,v 1.13 2003/10/31 16:30:15 scw Exp $	*/

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
 * RiscBSD kernel project
 *
 * exception.S
 *
 * Low level handlers for exception vectors
 *
 * Created      : 24/09/94
 *
 * Based on kate/display/abort.s
 *
 */

#include "assym.s"

#include <machine/asm.h>
#include <machine/armreg.h>
#include <machine/asmacros.h>
__FBSDID("$FreeBSD$");

	.text	
	.align	0

AST_LOCALS

/*
 * reset_entry:
 *
 *	Handler for Reset exception.
 */
ASENTRY_NP(reset_entry)
	adr	r0, Lreset_panicmsg
	bl	_C_LABEL(panic)
	/* NOTREACHED */
Lreset_panicmsg:
	.asciz	"Reset vector called, LR = 0x%08x"
	.balign	4
END(reset_entry)

/*
 * swi_entry
 *
 *	Handler for the Software Interrupt exception.
 */
ASENTRY_NP(swi_entry)
	STOP_UNWINDING			/* Don't unwind past here */

	PUSHFRAME

	mov	r0, sp			/* Pass the frame to any function */
	bl	_C_LABEL(swi_handler)	/* It's a SWI ! */

	DO_AST
	PULLFRAME
	movs	pc, lr			/* Exit */
END(swi_entry)

/*
 * prefetch_abort_entry:
 *
 *	Handler for the Prefetch Abort exception.
 */
ASENTRY_NP(prefetch_abort_entry)
#ifdef __XSCALE__
	nop				/* Make absolutely sure any pending */
	nop				/* imprecise aborts have occurred. */
#endif
        sub     lr, lr, #0x00000004     /* Adjust the lr */

	PUSHFRAMEINSVC
	ldr	r1, Lprefetch_abort_handler_address
	adr	lr, exception_exit
 	mov	r0, sp			/* pass the stack pointer as r0 */
	ldr	pc, [r1]

Lprefetch_abort_handler_address:
	.word	_C_LABEL(prefetch_abort_handler_address)

	.data
	.global	_C_LABEL(prefetch_abort_handler_address)

_C_LABEL(prefetch_abort_handler_address):
	.word	prefetch_abort_handler

END(prefetch_abort_entry)

/*
 * data_abort_entry:
 *
 *	Handler for the Data Abort exception.
 */
ASENTRY_NP(data_abort_entry)
#ifdef __XSCALE__
	nop				/* Make absolutely sure any pending */
	nop				/* imprecise aborts have occurred. */
#endif

        sub     lr, lr, #0x00000008     /* Adjust the lr */
	PUSHFRAMEINSVC			/* Push trap frame and switch */
					/* to SVC32 mode */
	ldr	r1, Ldata_abort_handler_address
	adr	lr, exception_exit
	mov	r0, sp			/* pass the stack pointer as r0 */
	ldr	pc, [r1]
Ldata_abort_handler_address:
	.word	_C_LABEL(data_abort_handler_address)

	.data
	.global	_C_LABEL(data_abort_handler_address)
_C_LABEL(data_abort_handler_address):
	.word	data_abort_handler

END(data_abort_entry)

/*
 * addr_exception_entry:
 *
 *	Handler for the Address Exception exception.
 *
 *	NOTE: This exception isn't really used on arm32.  We
 *	print a warning message to the console and then treat
 *	it like a Data Abort.
 */
ASENTRY_NP(addr_exception_entry)
	mrs	r1, cpsr
	mrs	r2, spsr
	mov	r3, lr
	adr	r0, Laddr_exception_msg
	bl	_C_LABEL(printf)	/* XXX CLOBBERS LR!! */
	b	data_abort_entry
Laddr_exception_msg:
	.asciz	"Address Exception CPSR=0x%08x SPSR=0x%08x LR=0x%08x\n"
	.balign	4
END(addr_exception_entry)

/*
 * General exception exit handler
 * (Placed here to be within range of all the references to it)
 *
 * It exits straight away if not returning to USR mode.
 * This loops around delivering any pending ASTs.
 * Interrupts are disabled at suitable points to avoid ASTs
 * being posted between testing and exit to user mode.
 *
 * This function uses PULLFRAMEFROMSVCANDEXIT and DO_AST and can
 * only be called if the exception handler used PUSHFRAMEINSVC.
 *
 * For EABI, don't try to unwind any further than this.  This is a
 * stopgap measure to avoid getting stuck in a loop in the unwinder,
 * which happens because we don't yet provide the proper unwind info
 * here that describes which registers are being restored.
 */

ASENTRY_NP(exception_exit)
	UNWINDSVCFRAME
	DO_AST
	PULLFRAMEFROMSVCANDEXIT
END(exception_exit)

ASENTRY_NP(irq_entry)
	sub	lr, lr, #0x00000004	/* Adjust the lr */
	PUSHFRAMEINSVC			/* Push an interrupt frame */
	mov	r0, sp			/* arg for dispatcher */

	adr	lr, exception_exit
	mov	r1, #0
	b	_C_LABEL(arm_irq_handler)
END(irq_entry)

/*
 * undefined_entry:
 *
 *	Handler for the Undefined Instruction exception.
 *
 *	We indirect the undefined vector via the handler address
 *	in the data area.  Entry to the undefined handler must
 *	look like direct entry from the vector.
 */
ASENTRY_NP(undefined_entry)

        sub     lr, lr, #0x00000004     /* Adjust the lr */
	PUSHFRAMEINSVC			/* Push trap frame and switch */
					/* to SVC32 mode */
	ldr	r1, Lundefined_handler_address
	adr	lr, exception_exit
	mov	r0, sp			/* pass the stack pointer as r0 */
	ldr	pc, [r1]
END(undefined_entry)

ASENTRY_NP(undefinedinstruction_bounce)
	b	undefinedinstruction
END(undefinedinstruction_bounce)

Lundefined_handler_address:
	.word	_C_LABEL(undefined_handler_address)

	.data
	.global	_C_LABEL(undefined_handler_address)
_C_LABEL(undefined_handler_address):
	.word	undefinedinstruction_bounce

/*
 * Entry point for FIQ interrupts.
 *
 * We don't currently support FIQ handlers very much.  Something can 
 * install itself in the FIQ vector using code (that may or may not work
 * these days) in fiq.c.  If nobody does that and an FIQ happens, this
 * default handler just disables FIQs and otherwise ignores it.
 */
ASENTRY_NP(fiq_entry)
	mrs	r8, cpsr		/* FIQ handling isn't supported, */
	bic	r8, #(F32_bit)		/* just disable FIQ and return.  */
	msr	cpsr_c, r8		/* The r8 we trash here is the  */
	subs	pc, lr, #4		/* banked FIQ-mode r8. */
END(fiq_entry)

/*
 * page0 and page0_data -- An image of the ARM vectors which is copied to
 * the ARM vectors page (high or low) as part of CPU initialization.  The
 * code that does the copy assumes that page0_data holds one 32-bit word
 * of data for each of the predefined ARM vectors.  It also assumes that
 * page0_data follows the vectors in page0, but other stuff can appear 
 * between the two.  We currently leave room between the two for some fiq 
 * handler code to be copied in.
 */
	.global	_C_LABEL(page0), _C_LABEL(page0_data)

_C_LABEL(page0):
	ldr	pc, .Lreset_entry
	ldr	pc, .Lundefined_entry
	ldr	pc, .Lswi_entry
	ldr	pc, .Lprefetch_abort_entry
	ldr	pc, .Ldata_abort_entry
	ldr	pc, .Laddr_exception_entry
	ldr	pc, .Lirq_entry
.fiqv:	ldr	pc, .Lfiq_entry
	.space 256	/* room for some fiq handler code */

_C_LABEL(page0_data):
.Lreset_entry:		.word	reset_entry
.Lundefined_entry:	.word	undefined_entry
.Lswi_entry:		.word	swi_entry
.Lprefetch_abort_entry:	.word	prefetch_abort_entry
.Ldata_abort_entry:	.word	data_abort_entry
.Laddr_exception_entry:	.word	addr_exception_entry
.Lirq_entry:		.word	irq_entry
.Lfiq_entry:		.word	fiq_entry

/*
 * These items are used by the code in fiq.c to install what it calls the
 * "null" handler.  It's actually our default vector entry that just jumps
 * to the default handler which just disables FIQs and returns.
 */
	.global _C_LABEL(fiq_nullhandler_code), _C_LABEL(fiq_nullhandler_size)

_C_LABEL(fiq_nullhandler_code):
	.word	.fiqv
_C_LABEL(fiq_nullhandler_size):
	.word	4


