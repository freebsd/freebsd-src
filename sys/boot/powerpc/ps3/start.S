/*-
 * Copyright (C) 2010 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <machine/trap_aim.h>

/*
 * KBoot and simulators will start this program from the _start symbol, with
 * r3 pointing to a flattened device tree (kexec), r4 the physical address
 * at which we were loaded, and r5 0 (kexec) or a pointer to Open Firmware
 * (simulator). If r4 is non-zero, the first order of business is relocating
 * ourselves to 0. In the kboot case, the PPE secondary thread will enter
 * at 0x60.
 *
 * If started directly by the LV1 hypervisor, we are loaded to address 0
 * and execution on both threads begins at 0x100 (EXC_RST).
 */

#define CACHELINE_SIZE			128
#define SPR_CTRL			136

/* KBoot thread 0 entry -- do relocation, then jump to main */
.global _start
_start:
	mfmsr   %r31
	clrldi  %r31,%r31,1
	mtmsrd  %r31
	isync
	cmpwi	%r4,0
	bne	relocate_self
relocated_start:
	lis	%r1,0x100
	bl	main

. = 0x40
.global secondary_spin_sem
secondary_spin_sem:
	.long	0

. = 0x60
thread1_start_kboot:
	mfmsr   %r31
	clrldi  %r31,%r31,1
	mtmsrd  %r31
	isync

	ba	thread1_start	/* kboot copies the first 256 bytes to
				 * address 0, so we are safe to jump
				 * (and stay) there */

thread1_start:
	li	%r3,secondary_spin_sem@l
1:	lwz	%r1,0(%r3)	/* Spin on SECONDARY_SPIN_SEM_ADDRESS */
	cmpwi	%r1,0
	beq	1b		/* If the semaphore is still zero, spin again */

	/* We have been woken up by thread 0 */
	li	%r0,0x100	/* Invalidate reset vector cache line */
	icbi	0,%r0
	isync
	sync
	ba	0x100		/* Jump to the reset vector */

. = EXC_RST
exc_rst:
	mfmsr   %r31
	clrldi  %r31,%r31,1
	mtmsrd  %r31
	isync

	mfspr	%r3,SPR_CTRL
	/* The first two bits of r0 are 01 (thread 1) or 10 (thread 0) */
	cntlzw	%r3,%r3		/* Now 0 for thread 0, 1 for thread 1 */

	cmpwi	%r3,0
	bne	thread1_start	/* Send thread 1 to wait */

	b	relocated_start /* Main entry point for thread 0 */

#define EXCEPTION_HANDLER(exc) \
. = exc; \
	li	%r3, exc; \
	mfsrr0	%r4; \
	mfmsr	%r5; \
	clrldi  %r6,%r5,1; \
	mtmsrd  %r6; \
	isync; \
	lis	%r1,0x100; \
	bl	ppc_exception

EXCEPTION_HANDLER(EXC_MCHK)
EXCEPTION_HANDLER(EXC_DSI)
EXCEPTION_HANDLER(EXC_DSE)
EXCEPTION_HANDLER(EXC_ISI)
EXCEPTION_HANDLER(EXC_ISE)
EXCEPTION_HANDLER(EXC_EXI)
EXCEPTION_HANDLER(EXC_ALI)
EXCEPTION_HANDLER(EXC_PGM)
EXCEPTION_HANDLER(EXC_FPU)
EXCEPTION_HANDLER(EXC_DECR)
EXCEPTION_HANDLER(EXC_SC)

relocate_self:
	/* We enter this with r4 the physical offset for our relocation */
	lis	%r8,_end@ha	/* r8: copy length */
	addi	%r8,%r8,_end@l
	li	%r5,0x100	/* r5: dest address */
1:	add	%r6,%r4,%r5	/* r6: source address */
	ld	%r7,0(%r6)
	std	%r7,0(%r5)
	addi	%r5,%r5,8
	cmpw	%r5,%r8
	blt	1b

	/*
	 * Now invalidate the cacheline with the second half of relocate_self,
	 * and do an absolute branch there in case we overwrote part of
	 * ourselves.
	 */
	
	lis	%r9,relocate_self_cache@ha
	addi	%r9,%r9,relocate_self_cache@l
	dcbst	0,%r9
	sync
	icbi	0,%r9
	sync
	isync
	ba	relocate_self_cache

relocate_self_cache:
	/* Now invalidate the icache */
	li	%r5,0x100
2:	dcbst	0,%r5
	sync
	icbi	0,%r5
	sync
	isync
	cmpw	%r5,%r8
	addi	%r5,%r5,CACHELINE_SIZE
	blt	2b

	/* All done: absolute jump to relocated entry point */
	ba	relocated_start

