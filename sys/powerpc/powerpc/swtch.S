/* $FreeBSD$ */
/* $NetBSD: locore.S,v 1.24 2000/05/31 05:09:17 thorpej Exp $ */

/*
 * Copyright (C) 2001 Benno Rice
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.s"

#include <sys/syscall.h>

#include <machine/trap.h>
#include <machine/param.h>
#include <machine/sr.h>
#include <machine/psl.h>
#include <machine/asm.h>

/*
 * void cpu_switch()
 * Find a runnable thread and switch to it.
 */
ENTRY(cpu_switch)
	mfsprg	%r3,0			/* Get the pcpu pointer */
	lwz	%r4,PC_CURTHREAD(%r3)	/* Get the current thread */
	lwz	%r3,TD_PCB(%r4)		/* Get a pointer to the PCB */

	stmw	%r14,PCB_CONTEXT(%r3)	/* Save the non-volatile GP regs */
	mr	%r14,%r3		/* Copy the curthread pointer */
	mr	%r15,%r4		/* ... and the current pcb pointer */
	mfcr	%r16			/* Save the condition register */
	stw	%r16,PCB_CR(%r3)
	mflr	%r16			/* Save the link register */
	stw	%r16,PCB_LR(%r3)
	stw	%r1,PCB_SP(%r3)		/* Save the stack pointer */

#if 0
	lwz	%r29,PCB_FLAGS(%r3)
	andi.	%r9, %r29, 1		/* XXX - don't hard code */
	beq	.L1
	bl	save_fpu
#endif

.L1:
	bl	choosethread		/* Find a new thread to run */
	cmplw	0,%r3,%r15		/* Compare to curthread */
	beq	.L2			/* If it's the same, we're done */

	mr	%r16,%r3		/* Save off the (struct thread *) */

	mr	%r3,%r15
	bl	pmap_deactivate		/* Deactivate the current pmap */

	mr	%r3,%r16
	bl	pmap_activate		/* Activate the new address space */

	mfsprg	%r4,0			/* Get the pcpu pointer */
	stw	%r16,PC_CURTHREAD(%r4)	/* Store new current thread */
	lwz	%r4,TD_PCB(%r16)	/* Grab the new PCB */

#if 0
	lwz	%r29, PCB_FLAGS(%r4)	/* Restore FPU regs if needed */
	andi.	%r9, %r29, 1
	beq	.L2
	mr	%r3, %r4
	bl	enable_fpu
#endif

.L2:
	lmw	%r14,PCB_CONTEXT(%r4)	/* Load the non-volatile GP regs */
	lwz	%r5,PCB_CR(%r4)		/* Load the condition register */
	mtcr	%r5
	lwz	%r5,PCB_LR(%r4)		/* Load the link register */
	mtlr	%r5
	lwz	%r1,PCB_SP(%r4)		/* Load the stack pointer */
	blr

/*
 * savectx(pcb)
 * Update pcb, saving current processor state
 */
ENTRY(savectx)
	stmw	%r14,PCB_CONTEXT(%r3)	/* Save the non-volatile GP regs */
	mfcr	%r4			/* Save the condition register */
	stw	%r4,PCB_CONTEXT(%r3)
	blr

/*
 * fork_trampoline()
 * Set up the return from cpu_fork()
 */
ENTRY(fork_trampoline)
	lwz	%r3,CF_FUNC(1)
	lwz	%r4,CF_ARG0(1)
	lwz	%r5,CF_ARG1(1)
	bl	fork_exit
	rfi
