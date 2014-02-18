/*-
 * Copyright (c) 2012-2014 Andrew Turner
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
 *
 * $FreeBSD$
 */

#include "assym.s"
#include <machine/armreg.h>
#include <machine/hypervisor.h>

	.globl	kernbase
	.set	kernbase, KERNBASE

/*
 * We assume:
 *  MMU      off
 *  D-Cache: off
 *  I-Cache: on or off
 *  We are loaded at a 2MiB aligned address
 */

#define	INIT_STACK_SIZE	(PAGE_SIZE * 4)

	.text
	.globl _start
_start:
	/* Drop to EL1 */
	bl	drop_to_el1

	/* Load the address of the fvp UART */
	mov	x0, 0x1c090000
	/* Load 'A' */
	mov	x1, 0x41
	/* Print 'A' */
	str	x1, [x0]

1:	b	1b

/*
 * If we are started in EL2, configure the required hypervisor
 * registers and drop to EL1.
 */
drop_to_el1:
	mrs	x1, CurrentEL
	lsr	x1, x1, #2
	cmp	x1, #0x2
	b.eq	1f
	ret
1:
	/* Configure the Hypervisor */
	mov	x2, #(HCR_RW)
	msr	hcr_el2, x2

	/* Load the Virtualization Process ID Register */
	mrs	x2, midr_el1
	msr	vpidr_el2, x2

	/* Load the Virtualization Multiprocess ID Register */
	mrs	x2, mpidr_el1
	msr	vmpidr_el2, x2

	/* Set the bits that need to be 1 in sctlr_el1 */
	ldr	x2, .Lsctlr_res1
	msr	sctlr_el1, x2

	/* Don't trap to EL2 for exceptions */
	mov	x2, #CPTR_RES1
	msr	cptr_el2, x2

	/* Don't trap to EL2 for CP15 traps */
	msr	hstr_el2, xzr

	/* Hypervisor trap functions */
	adr	x2, hyp_vectors
	msr	vbar_el2, x2

	mov	x2, #(PSR_F | PSR_I | PSR_A | PSR_D | PSR_M_EL1h)
	msr	spsr_el2, x2

	/* Set the address to return to */
	msr	elr_el2, x30

	eret
.Lsctlr_res1:
	.word SCTLR_RES1

#define	VECT_EMPTY	\
    .align 7;		\
    1:	b	1b

	.align 11
hyp_vectors:
	VECT_EMPTY	/* Synchronous EL2t */
	VECT_EMPTY	/* IRQ EL2t */
	VECT_EMPTY	/* FIQ EL2t */
	VECT_EMPTY	/* Error EL2t */

	VECT_EMPTY	/* Synchronous EL2h */
	VECT_EMPTY	/* IRQ EL2h */
	VECT_EMPTY	/* FIQ EL2h */
	VECT_EMPTY	/* Error EL2h */

	VECT_EMPTY	/* Synchronous 64-bit EL1 */
	VECT_EMPTY	/* IRQ 64-bit EL1 */
	VECT_EMPTY	/* FIQ 64-bit EL1 */
	VECT_EMPTY	/* Error 64-bit EL1 */

	VECT_EMPTY	/* Synchronous 32-bit EL1 */
	VECT_EMPTY	/* IRQ 32-bit EL1 */
	VECT_EMPTY	/* FIQ 32-bit EL1 */
	VECT_EMPTY	/* Error 32-bit EL1 */

hyp_trap_invalid:
	b	hyp_trap_invalid

	.globl abort
abort:
	b abort
