/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/pstate.h>

#include "assym.s"

	.register %g2,#ignore

	.globl	kernbase
	.set	kernbase,KERNBASE
	
	.globl	nwin_minus_one
	.set	nwin_minus_one,7
	
/*
 * void _start(caddr_t metadata, u_long o1, u_long o2, u_long o3,
 *	       u_long ofw_vec)
 */
ENTRY(btext)
ENTRY(_start)
	/*
	 * Initialize misc state to known values.  Interrupts disabled, normal
	 * globals, windows flushed (cr = 0, cs = nwindows - 1), no clean
	 * windows, pil 0, and floating point disabled.
	 */
	wrpr	%g0, PSTATE_NORMAL, %pstate
	flushw
	wrpr	%g0, 0, %cleanwin
	wrpr	%g0, 0, %pil
	wr	%g0, 0, %fprs

	/*
	 * Get onto our per-cpu panic stack, which precedes the struct pcpu in
	 * the per-cpu page.
	 */
	SET(pcpu0 + (PCPU_PAGES * PAGE_SIZE) - PC_SIZEOF, %l1, %l0)
	sub	%l0, SPOFF + CCFSZ, %sp

	/*
	 * Do initial bootstrap to setup pmap and thread0.
	 */
	call	sparc64_init
	 nop

	GET_PCB(%g6)
	/*
	 * Get onto thread0's kstack.
	 */
	ldx	[%g6 + PCB_KSTACK], %sp

	/*
	 * And away we go.  This doesn't return.
	 */
	call	mi_startup
	 nop
	sir
	! NOTREACHED
END(_start)

/*
 * void cpu_setregs(struct pcpu *pc)
 */
ENTRY(cpu_setregs)

	/*
	 * Disable interrupts, normal globals.
	 */
	mov	%o0, PCPU_REG

	/*
	 * store PCPU_REG into its scratchpad register
	 * this allows us to retrieve it in the trap handlers - 
	 * where we receive a new set of globals
	 */
	mov	SCRATCH_REG_PCPU, %g2 
	stxa	PCPU_REG, [%g2]ASI_SCRATCHPAD 

	retl
	 nop
END(cpu_setregs)
