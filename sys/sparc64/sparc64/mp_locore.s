/*-
 * Copyright (c) 2002 Jake Burkholder.
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

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/ktr.h>
#include <machine/pstate.h>

#include "assym.s"

	.register	%g2, #ignore
	.register	%g3, #ignore

/*
 * void _mp_start(u_long o0, u_int *state, u_int mid, u_long o3, u_long o4)
 */
ENTRY(_mp_start)
	/*
	 * Give away our stack to another processor that may be starting in the
	 * loader.
	 */
	clr	%sp

	/*
	 * Inform the boot processor which is waiting in the loader that we
	 * made it.
	 */
	mov	CPU_INITED, %l0
	stw	%l0, [%o1]
	membar	#StoreLoad

#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP, "_mp_start: cpu %d entered kernel"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o2, [%g1 + KTR_PARM1]
9:
#endif

	SET(cpu_start_args, %l1, %l0)

	/*
	 * Wait till its our turn to start.
	 */
1:	membar	#StoreLoad
	lduw	[%l0 + CSA_MID], %l1
	cmp	%l1, %o2
	bne	%xcc, 1b
	 nop

#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP, "_mp_start: cpu %d got start signal"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o2, [%g1 + KTR_PARM1]
9:
#endif

	/*
	 * Find our per-cpu page and the tte data that we will use to map it.
	 */
	ldx	[%l0 + CSA_DATA], %l1
	ldx	[%l0 + CSA_VA], %l2

	/*
	 * Map the per-cpu page.  It uses a locked tlb entry.
	 */
	wr	%g0, ASI_DMMU, %asi
	stxa	%l2, [%g0 + AA_DMMU_TAR] %asi
	stxa	%l1, [%g0] ASI_DTLB_DATA_IN_REG
	membar	#Sync

	/*
	 * Get onto our per-cpu panic stack, which precedes the struct pcpu
	 * in the per-cpu page.
	 */
	set	PAGE_SIZE - PC_SIZEOF, %l3
	add	%l2, %l3, %l2
	sub	%l2, SPOFF + CCFSZ, %sp

	/*
	 * Inform the boot processor that we're about to start.
	 */
	mov	CPU_STARTED, %l3
	stw	%l3, [%l0 + CSA_STATE]
	membar	#StoreLoad

	/*
	 * Enable interrupts.
	 */
	wrpr	%g0, PSTATE_KERNEL, %pstate

#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP,
	    "_mp_start: bootstrap cpuid=%d mid=%d pcpu=%#lx data=%#lx sp=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	lduw	[%l2 + PC_CPUID], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	lduw	[%l2 + PC_MID], %g2
	stx	%g2, [%g1 + KTR_PARM2]
	stx	%l2, [%g1 + KTR_PARM3]
	stx	%l1, [%g1 + KTR_PARM4]
	stx	%sp, [%g1 + KTR_PARM5]
9:
#endif

	/*
	 * And away we go.  This doesn't return.
	 */
	call	cpu_mp_bootstrap
	 mov	%l2, %o0
	sir
	! NOTREACHED
END(_mp_start)
