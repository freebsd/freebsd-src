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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/sys/sparc64/sparc64/mp_locore.S,v 1.8 2007/01/19 11:15:33 marius Exp $");

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/ktr.h>
#include <machine/pstate.h>
#include <machine/smp.h>
#include <machine/upa.h>

#include "assym.s"

	.register	%g2, #ignore
	.register	%g3, #ignore

	.text
	_ALIGN_TEXT
1:	rd	%pc, %l0
	ldx	[%l0 + (4f-1b)], %l1
	add	%l0, (6f-1b), %l2
	clr	%l3
2:	cmp	%l3, %l1
	be	%xcc, 3f
	 nop
	ldx	[%l2 + TTE_VPN], %l4
	ldx	[%l2 + TTE_DATA], %l5
	srlx	%l4, TV_SIZE_BITS, %l4
	sllx	%l4, PAGE_SHIFT_4M, %l4
	wr	%g0, ASI_DMMU, %asi
	stxa	%l4, [%g0 + AA_DMMU_TAR] %asi
	stxa	%l5, [%g0] ASI_DTLB_DATA_IN_REG
	wr	%g0, ASI_IMMU, %asi
	stxa	%l4, [%g0 + AA_IMMU_TAR] %asi
	stxa	%l5, [%g0] ASI_ITLB_DATA_IN_REG
	membar	#Sync
	flush	%l4
	add	%l2, 1 << TTE_SHIFT, %l2
	add	%l3, 1, %l3
	ba	%xcc, 2b
	 nop
3:	ldx	[%l0 + (5f-1b)], %l1
	jmpl	%l1, %g0
	 nop
	_ALIGN_DATA
4:	.xword	0x0
5:	.xword	0x0
6:

DATA(mp_tramp_code)
	.xword	1b
DATA(mp_tramp_code_len)
	.xword	6b-1b
DATA(mp_tramp_tlb_slots)
	.xword	4b-1b
DATA(mp_tramp_func)
	.xword	5b-1b

/*
 * void mp_startup(void)
 */
ENTRY(mp_startup)
	wrpr	%g0, PSTATE_NORMAL, %pstate
	wrpr	%g0, 0, %cleanwin
	wrpr	%g0, 0, %pil
	wr	%g0, 0, %fprs

	SET(cpu_start_args, %l1, %l0)

	mov	CPU_CLKSYNC, %l1
	membar	#StoreLoad
	stw	%l1, [%l0 + CSA_STATE]

1:	ldx	[%l0 + CSA_TICK], %l1
	brz	%l1, 1b
	 nop
	wrpr	%l1, 0, %tick

	UPA_GET_MID(%o0)

#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP, "mp_start: cpu %d entered kernel"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif

	rdpr	%ver, %l1
	stx	%l1, [%l0 + CSA_VER]

	/*
	 * Inform the boot processor we have inited.
	 */
	mov	CPU_INIT, %l1
	membar	#LoadStore
	stw	%l1, [%l0 + CSA_STATE]

	/*
	 * Wait till its our turn to bootstrap.
	 */
2:	lduw	[%l0 + CSA_MID], %l1
	cmp	%l1, %o0
	bne	%xcc, 2b
	 nop

#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP, "_mp_start: cpu %d got start signal"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif

	add	%l0, CSA_TTES, %l1
	clr	%l2

	/*
	 * Map the per-cpu pages.
	 */
3:	sllx	%l2, TTE_SHIFT, %l3
	add	%l1, %l3, %l3

	ldx	[%l3 + TTE_VPN], %l4
	ldx	[%l3 + TTE_DATA], %l5

	wr	%g0, ASI_DMMU, %asi
	srlx	%l4, TV_SIZE_BITS, %l4
	sllx	%l4, PAGE_SHIFT_8K, %l4
	stxa	%l4, [%g0 + AA_DMMU_TAR] %asi
	stxa	%l5, [%g0] ASI_DTLB_DATA_IN_REG
	membar	#Sync

	add	%l2, 1, %l2
	cmp	%l2, PCPU_PAGES
	bne	%xcc, 3b
	 nop

	/*
	 * Get onto our per-cpu panic stack, which precedes the struct pcpu
	 * in the per-cpu page.
	 */
	ldx	[%l0 + CSA_PCPU], %l1
	set	PCPU_PAGES * PAGE_SIZE - PC_SIZEOF, %l2
	add	%l1, %l2, %l1
	sub	%l1, SPOFF + CCFSZ, %sp

	/*
	 * Enable interrupts.
	 */
	wrpr	%g0, PSTATE_KERNEL, %pstate

#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP,
	    "_mp_start: bootstrap cpuid=%d mid=%d pcpu=%#lx data=%#lx sp=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	lduw	[%l1 + PC_CPUID], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	lduw	[%l1 + PC_MID], %g2
	stx	%g2, [%g1 + KTR_PARM2]
	stx	%l1, [%g1 + KTR_PARM3]
	stx	%sp, [%g1 + KTR_PARM5]
9:
#endif

	/*
	 * And away we go.  This doesn't return.
	 */
	call	cpu_mp_bootstrap
	 mov	%l1, %o0
	sir
	! NOTREACHED
END(mp_startup)
