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
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/asm.h>

/*
 * Some instructions gas doesn't understand (yet?)
 */
#define	bdneq	bdnzf 2,

/*
 * No processes are runnable, so loop waiting for one.
 * Separate label here for accounting purposes.
 */
#if 0 /* XXX: I think this is now unneeded.  Leaving it in just in case. */
ASENTRY(Idle)
	mfmsr	3
	andi.	3,3,~PSL_EE@l		/* disable interrupts while
					   manipulating runque */
	mtmsr	3

	lis	8,sched_whichqs@ha
	lwz	9,sched_whichqs@l(8)

	or.	9,9,9
	bne-	.Lsw1			/* at least one queue non-empty */
	
	ori	3,3,PSL_EE@l		/* reenable ints again */
	mtmsr	3
	isync

/* Check if we can use power saving mode */
	lis	8,powersave@ha
	lwz	9,powersave@l(8)

	or.	9,9,9
	beq	1f

	sync
	oris	3,3,PSL_POW@h		/* enter power saving mode */
	mtmsr	3
	isync
1:
	b	_ASM_LABEL(Idle)
#endif /* XXX */

/*
 * switchexit gets called from cpu_exit to complete the exit procedure.
 */
ENTRY(switchexit)
/* First switch to the idle pcb/kernel stack */
#if 0 /* XXX */
	lis	6,idle_u@ha
	lwz	6,idle_u@l(6)
	mfsprg	7,0
	stw	6,GD_CURPCB(7)
#endif
	addi	1,6,USPACE-16		/* 16 bytes are reserved at stack top */
	/*
	 * Schedule the vmspace and stack to be freed (the proc arg is
	 * already in r3).
	 */
	bl	sys_exit

/* Fall through to cpu_switch to actually select another proc */
	li	3,0			/* indicate exited process */

/*
 * void cpu_switch(struct proc *p)
 * Find a runnable process and switch to it.
 */
/* XXX noprofile?  --thorpej@netbsd.org */
ENTRY(cpu_switch)
	mflr	0			/* save lr */
	stw	0,4(1)
	stwu	1,-16(1)
	stw	31,12(1)
	stw	30,8(1)

	mr	30,3
	mfsprg	3,0
	xor	31,31,31
	stw	31,GD_CURPROC(3)	/* Zero to not accumulate cpu time */
	mfsprg	3,0
	lwz	31,GD_CURPCB(3)

	xor	3,3,3
#if 0 /* XXX */
	bl	lcsplx
#endif
	stw	3,PCB_SPL(31)		/* save spl */

/* Find a new process */
	bl	chooseproc

1:
	/* just did this resched thing */
	xor	3,3,3
	lis	4,want_resched@ha
	stw	3,want_resched@l(4)

	/* record new process */
	mfsprg	4,0
	stw	3,GD_CURPROC(4)

	cmpl	0,31,30			/* is it the same process? */
	beq	switch_return

	or.	30,30,30		/* old process was exiting? */
	beq	switch_exited

	mfsr	10,USER_SR		/* save USER_SR for copyin/copyout */
	mfcr	11			/* save cr */
	mr	12,2			/* save r2 */
	stwu	1,-SFRAMELEN(1)		/* still running on old stack */
	stmw	10,8(1)
	lwz	3,P_ADDR(30)
	stw	1,PCB_SP(3)		/* save SP */

switch_exited:
	mfmsr	3
	andi.	3,3,~PSL_EE@l		/* disable interrupts while
					   actually switching */
	mtmsr	3

	/* indicate new pcb */
	lwz	4,P_ADDR(31)
	mfsprg	5,0
	stw	4,GD_CURPCB(5)

#if 0 /* XXX */
	/* save real pmap pointer for spill fill */
	lwz	5,PCB_PMR(4)
	lis	6,curpm@ha
	stwu	5,curpm@l(6)
	stwcx.	5,0,6			/* clear possible reservation */
#endif

	addic.	5,5,64
	li	6,0
	mfsr	8,KERNEL_SR		/* save kernel SR */
1:
	addis	6,6,-0x10000000@ha	/* set new procs segment registers */
	or.	6,6,6			/* This is done from the real
					   address pmap */
	lwzu	7,-4(5)			/* so we don't have to worry */
	mtsrin	7,6			/* about accessibility */
	bne	1b
	mtsr	KERNEL_SR,8		/* restore kernel SR */
	isync

	lwz	1,PCB_SP(4)		/* get new procs SP */

	ori	3,3,PSL_EE@l		/* interrupts are okay again */
	mtmsr	3

	lmw	10,8(1)			/* get other regs */
	lwz	1,0(1)			/* get saved SP */
	mr	2,12			/* get saved r2 */
	mtcr	11			/* get saved cr */
	isync
	mtsr	USER_SR,10		/* get saved USER_SR */
	isync

switch_return:
	mr	30,7			/* save proc pointer */
	lwz	3,PCB_SPL(4)
#if 0 /* XXX */
	bl	lcsplx
#endif

	mr	3,30			/* get curproc for special fork
					   returns */

	lwz	31,12(1)
	lwz	30,8(1)
	addi	1,1,16
	lwz	0,4(1)
	mtlr	0
	blr

/*
 * Fake savectx for the time being.
 */
ENTRY(savectx)
	blr
