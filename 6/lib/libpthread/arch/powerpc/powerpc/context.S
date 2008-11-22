/*
 * Copyright (c) 2004 Peter Grehan.
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

#include "assym.s"

/*
 * int _ppc32_getcontext(mcontext_t *mcp)
 *
 *   Save register state from a voluntary context switch.
 * Only volatile registers, and those needed to complete
 * a setcontext call, need to be saved.
 *
 *	r1
 *	r14-31
 *	f14-31		XXX
 *	lr
 *
 *  Return 0 for this call, and set up the context so it will return
 * 1 when restored with _ppc32_setcontext().
 *
 * XXX XXX
 *  Floating-point is a big issue. Since there's no way to determine
 * if the caller has used FP, all volatile register need to be saved.
 * If FP hasn't been used, this results in a lazy FP exception in
 * the kernel and from that point on FP is always switched in/out
 * for the thread, which may be a big performance drag for the system.
 *  An alternative is to use a system call to get the context, which
 * will do the right thing for floating point, but will save all
 * registers rather than the caller-saved subset, and has the overhead
 * of a syscall.
 *  Maybe another option would be to give a light-weight way for a
 * thread to determine if FP is in used: perhaps a syscall that
 * returns in the asm traphandler, or an OSX-style read-only page
 * with a flag to indicate FP state.
 *
 * For now, punt the issue ala Alpha 1:1 model and fix in the future.
 */
ENTRY(_ppc32_getcontext)
	stw	%r1,  _MC_R1(%r3)
	stw	%r13, _MC_R13(%r3)
	stw	%r14, _MC_R14(%r3)
	stw	%r15, _MC_R15(%r3)
	stw	%r16, _MC_R16(%r3)
	stw	%r17, _MC_R17(%r3)
	stw	%r18, _MC_R18(%r3)
	stw	%r19, _MC_R19(%r3)
	stw	%r20, _MC_R20(%r3)
	stw	%r21, _MC_R21(%r3)
	stw	%r22, _MC_R22(%r3)
	stw	%r23, _MC_R23(%r3)
	stw	%r24, _MC_R24(%r3)
	stw	%r25, _MC_R25(%r3)
	stw	%r26, _MC_R26(%r3)
	stw	%r27, _MC_R27(%r3)
	stw	%r28, _MC_R28(%r3)
	stw	%r29, _MC_R28(%r3)
	stw	%r30, _MC_R30(%r3)
	stw	%r31, _MC_R31(%r3)
	mflr	%r4
	stw	%r4,  _MC_LR(%r3)
	mfcr	%r4
	stw	%r4,  _MC_CR(%r3)

	/* XXX f14-31 ? */

	li	%r4, _MC_VERSION_KSE	/* partial ucontext version */
	stw	%r4, _MC_VERS(%r3)

	/* Return 0 */
	li	%r3, 0
	blr

/*
 *	int _ppc32_setcontext(const mcontext_t *mcp, intptr_t val,
 *	    intptr_t *loc);
 *
 *  Should only be called for partial KSE contexts. The full context
 * case is handled by kse_switchin() in _thread_switch()
 *
 *  Returns -1 on error and 1 for return from a saved context
 */

ENTRY(_ppc32_setcontext)
	lwz	%r6, _MC_VERS(%r3)
	cmpwi	%r6, _MC_VERSION_KSE	/* KSE partial context ? */
	beq	1f
	li	%r3, -1			/* invalid context type, return -1 */
	blr

1:	/* partial format, callee-saved regs assumed */
	lwz	%r1,  _MC_R1(%r3)
	lwz	%r13, _MC_R13(%r3)
	lwz	%r14, _MC_R14(%r3)
	lwz	%r15, _MC_R15(%r3)
	lwz	%r16, _MC_R16(%r3)
	lwz	%r17, _MC_R17(%r3)
	lwz	%r18, _MC_R18(%r3)
	lwz	%r19, _MC_R19(%r3)
	lwz	%r20, _MC_R20(%r3)
	lwz	%r21, _MC_R21(%r3)
	lwz	%r22, _MC_R22(%r3)
	lwz	%r23, _MC_R23(%r3)
	lwz	%r24, _MC_R24(%r3)
	lwz	%r25, _MC_R25(%r3)
	lwz	%r26, _MC_R26(%r3)
	lwz	%r27, _MC_R27(%r3)
	lwz	%r28, _MC_R28(%r3)
	lwz	%r29, _MC_R28(%r3)
	lwz	%r30, _MC_R30(%r3)
	lwz	%r31, _MC_R31(%r3)
	lwz	%r6,  _MC_LR(%r3)
	mtlr	%r6
	lwz	%r6,  _MC_CR(%r3)
	mtcr	%r6

	/*  XXX f14-31 ? */

	/* if (loc != NULL) *loc = val */
	cmpwi	%r5, 0
	beq	2f
	stw	%r4, 0(%r5)

	/* Return 1 */
2:	li	%r3, 1
	blr
