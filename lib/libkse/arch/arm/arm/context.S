/*
 * Copyright (c) Olivier Houchard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * int thr_setcontext(mcontext_t *mcp, intptr_t val, intptr_t *loc)
 *
 * Restores the context in mcp.
 *
 * Returns 0 if there are no errors; -1 otherwise
 */
      
.weak _C_LABEL(_thr_setcontext)
.set _C_LABEL(_thr_setcontext), _C_LABEL(__thr_setcontext)

ENTRY(__thr_setcontext)
/* Check for NULL pointer. */
	cmp 	r0, #0
	moveq 	r0, #-1
	moveq 	pc, lr
	cmp	r2, #0
	strne	r1, [r2]
	ldr	r1, [r0, #(16 * 4)] /* CPSR */
	msr	cpsr, r1
	ldmia	r0, {r0-r15}
	mov	pc, lr
	/* XXX: FP bits ? */

/*
 * int thr_getcontext(mcontext_t *mcp);
 *
 * Returns -1 if there is an error, 0 no errors; 1 upon return
 * from a setcontext().
 */
.weak _C_LABEL(_thr_getcontext)
.set _C_LABEL(_thr_getcontext), _C_LABEL(__thr_getcontext)
		     
ENTRY(__thr_getcontext)
/* Check for NULL pointer. */
	cmp	r0, #0
	moveq	r0, #-1
	moveq	pc, lr
	stmia	r0, {r1-r14}
	mov	r1, #1
	str	r1, [r0] /* Return 1 from setcontext */
	str	lr, [r0, #(15 * 4)] /* PC */
	mrs	r1, cpsr
	str	r1, [r0, #(16 * 4)] /* CPSR */
	mov	r0, #0 /* Return 0. */
	mov	pc, lr

ENTRY(_arm_enter_uts)
	add	sp, r2, r3 /* Stack addr + size. */
	mov	pc, r1
