/*	$OpenBSD: locore.S,v 1.18 1998/09/15 10:58:53 pefo Exp $	*/
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Digital Equipment Corporation and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) 1989 Digital Equipment Corporation.
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * Digital Equipment Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/loMem.s,
 *	v 1.1 89/07/11 17:55:04 nelson Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAsm.s,
 *	v 9.2 90/01/29 18:00:39 shirriff Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/vm/ds3100.md/vmPmaxAsm.s,
 *	v 1.1 89/07/10 14:27:41 nelson Exp  SPRITE (DECWRL)
 *
 *	from: @(#)locore.s	8.5 (Berkeley) 1/4/94
 *	JNPR: locore.S,v 1.6.2.1 2007/08/29 12:24:49 girish
 * $FreeBSD$
 */

/*
 * FREEBSD_DEVELOPERS_FIXME
 * The start routine below was written for a multi-core CPU
 * with each core being hyperthreaded. This serves as an example
 * for a complex CPU architecture. For a different CPU complex
 * please make necessary changes to read CPU-ID etc.
 * A clean solution would be to have a different locore file for
 * each CPU type.
 */

/*
 *	Contains code that is the first executed at boot time plus
 *	assembly language support routines.
 */

#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/cpuregs.h>
#include <machine/regnum.h>

#include "assym.s"

	.data
#ifdef YAMON
GLOBAL(fenvp)
	.space 4			# Assumes mips32?  Is that OK?
#endif
GLOBAL(stackspace)
	.space NBPG /* Smaller than it should be since it's temp. */
	.align 8
GLOBAL(topstack)

	
	.set noreorder
	
	.text

GLOBAL(btext)
ASM_ENTRY(_start)
VECTOR(_locore, unknown)
	/* UNSAFE TO USE a0..a3, since some bootloaders pass that to us */
	mtc0	zero, COP_0_CAUSE_REG	# Clear soft interrupts
	
#if defined(TARGET_OCTEON)
	/*
	 * t1: Bits to set explicitly:
	 *	Enable FPU
	 */

	/* Set these bits */
        li	t1, (MIPS_SR_COP_2_BIT | MIPS_SR_COP_0_BIT | MIPS_SR_PX | MIPS_SR_KX | MIPS_SR_UX | MIPS_SR_SX | MIPS_SR_BEV)

	/* Reset these bits */
        li	t0, ~(MIPS_SR_DE | MIPS_SR_SOFT_RESET | MIPS_SR_ERL | MIPS_SR_EXL | MIPS_SR_INT_IE)
#else
	/*
	 * t0: Bits to preserve if set:
	 * 	Soft reset
	 *	Boot exception vectors (firmware-provided)
	 */
	li	t0, (MIPS_SR_BEV | MIPS_SR_SOFT_RESET)
	/*
	 * t1: Bits to set explicitly:
	 *	Enable FPU
	 */
	li	t1, MIPS_SR_COP_1_BIT
#endif
	/*
	 * Read coprocessor 0 status register, clear bits not
	 * preserved (namely, clearing interrupt bits), and set
	 * bits we want to explicitly set.
	 */
	mfc0	t2, COP_0_STATUS_REG
	and	t2, t0
	or	t2, t1
	mtc0	t2, COP_0_STATUS_REG
	COP0_SYNC
	/* Make sure KSEG0 is cached */
	li	t0, CFG_K0_CACHED
	mtc0	t0, MIPS_COP_0_CONFIG
	COP0_SYNC

	/* Read and store the PrID FPU ID for CPU identification, if any. */
	mfc0	t2, COP_0_STATUS_REG
	mfc0	t0, MIPS_COP_0_PRID
#ifdef CPU_HAVEFPU
	and	t2, MIPS_SR_COP_1_BIT
	beqz	t2, 1f
	move	t1, zero
	cfc1	t1, MIPS_FPU_ID
1:
#else
	/*
	 * This platform has no FPU, and attempting to detect one
	 * using the official method causes an exception.
	 */
	move	t1, zero
#endif
	sw	t0, _C_LABEL(cpu_id)
	sw	t1, _C_LABEL(fpu_id)

/*
 * Initialize stack and call machine startup.
 */
	PTR_LA	sp, _C_LABEL(topstack) - START_FRAME
	PTR_LA	gp, _C_LABEL(_gp)
	sw	zero, START_FRAME - 4(sp)  # Zero out old ra for debugger

	/*xxximp
	 * now that we pass a0...a3 to the platform_init routine, do we need
	 * to stash this stuff here?
	 */
#ifdef YAMON
	/* Save YAMON boot environment pointer */
	sw	a2, _C_LABEL(fenvp)
#endif

	/*
	 * The following needs to be done differently for each platform and
	 * there needs to be a good way to plug this in.
	 */
#if defined(SMP) && defined(CPU_XLR)
/*
 * Block all the slave CPUs
 */
	/* XXX a0, a1, a2 shouldn't be used here */
	/*
	 * Read the cpu id from the cp0 config register
	 * cpuid[9:4], thrid[3: 0]
	 */
	mfc0	a0, COP_0_CONFIG, 7
	srl	a1, a0, 4
	andi	a1, a1, 0x3f
	andi	a0, a0, 0xf

	/* calculate linear cpuid */
	sll     t0, a1, 2
	addu    a2, t0, a0
/* Initially, disable all hardware threads on each core except thread0 */
	li	t1, VCPU_ID_0
	li	t2, XLR_THREAD_ENABLE_IND
	mtcr	t1, t2
#endif


#if defined(TARGET_OCTEON) /* Maybe this is mips32/64 generic? */
	.set push
	.set mips32r2
	rdhwr	t0, $0
	.set pop
#else
	move	t0, zero
#endif

	/* Stage the secondary cpu start until later */
	bne	t0, zero, start_secondary
	nop

#ifdef SMP
	PTR_LA	t0, _C_LABEL(__pcpu)
	SET_CPU_PCPU(t0)
	/* If not master cpu, jump... */
/*XXX this assumes the above #if 0'd code runs */
	bne    a2, zero, start_secondary
	nop
#endif

	/* Call the platform-specific startup code. */
	jal	_C_LABEL(platform_start)
	sw	zero, START_FRAME - 8(sp)	# Zero out old fp for debugger

	PTR_LA	sp, _C_LABEL(thread0)
	lw      a0, TD_PCB(sp)
	li	t0, ~7
	and	a0, a0, t0
	subu    sp, a0, START_FRAME

	jal	_C_LABEL(mi_startup)		# mi_startup(frame)
	sw	zero, START_FRAME - 8(sp)	# Zero out old fp for debugger

	PANIC("Startup failed!")

#ifdef SMP
start_secondary:
	move	a0, a1
2:
	addiu	t0, PCPU_SIZE
	subu	a1, 1
	bne	a1, zero, 2b
	nop
	SET_CPU_PCPU(t0)
smp_wait:
	lw	sp, PC_BOOT_STACK(t0)
	beqz	sp, smp_wait
	nop
	jal	_C_LABEL(smp_init_secondary)
	nop
#else
start_secondary:
	b	start_secondary
	nop
#endif

VECTOR_END(_locore)
