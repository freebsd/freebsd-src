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
#include <machine/ktr.h>
#include <machine/asmacros.h>
#include <machine/pstate.h>

#include "assym.s"

	.register	%g2, #ignore
	.register	%g3, #ignore

#define	IPI_WAIT(r1, r2, r3) \
	ATOMIC_DEC_INT(r1, r2, r3) ; \
9:	membar	#StoreLoad ; \
	lduw	[r1], r2 ; \
	brnz,a,pn r2, 9b ; \
	 nop

/*
 * Trigger a softint at the desired level.
 */
ENTRY(tl_ipi_level)
	lduw	[%g5 + ILA_LEVEL], %g2

	mov	1, %g1
	sllx	%g1, %g2, %g1
	wr	%g1, 0, %asr20

	IPI_WAIT(%g5, %g1, %g2)
	retry
END(tl_ipi_level)

ENTRY(tl_ipi_test)
#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP, "ipi_test: cpuid=%d mid=%d d1=%#lx d2=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	lduw	[PCPU(CPUID)], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	lduw	[PCPU(MID)], %g2
	stx	%g2, [%g1 + KTR_PARM2]
	stx	%g4, [%g1 + KTR_PARM3]
	stx	%g5, [%g1 + KTR_PARM4]
9:
#endif
	retry
END(tl_ipi_test)

/*
 * Demap a page from the dtlb and/or itlb.
 */
ENTRY(tl_ipi_tlb_page_demap)
	ldx	[%g5 + ITA_TLB], %g1
	ldx	[%g5 + ITA_CTX], %g2
	ldx	[%g5 + ITA_VA], %g3

	wr	%g0, ASI_DMMU, %asi

	brz,a,pt %g2, 1f
	 or	%g3, TLB_DEMAP_NUCLEUS | TLB_DEMAP_PAGE, %g3

	stxa	%g2, [%g0 + AA_DMMU_SCXR] %asi
	membar	#Sync
	or	%g3, TLB_DEMAP_SECONDARY | TLB_DEMAP_PAGE, %g3

1:	andcc	%g1, TLB_DTLB, %g0
	bz,a,pn %xcc, 2f
	 nop
	stxa	%g0, [%g3] ASI_DMMU_DEMAP

2:	andcc	%g1, TLB_ITLB, %g0
	bz,a,pn %xcc, 3f
	 nop
	stxa	%g0, [%g3] ASI_IMMU_DEMAP

3:	brz,a,pt %g2, 4f
	 nop
	stxa	%g0, [%g0 + AA_DMMU_SCXR] %asi

4:	membar	#Sync

	IPI_WAIT(%g5, %g1, %g2)
	retry
END(tl_ipi_tlb_page_demap)

/*
 * Demap a range of pages from the dtlb and itlb.
 */
ENTRY(tl_ipi_tlb_range_demap)
	ldx	[%g5 + ITA_CTX], %g1
	ldx	[%g5 + ITA_START], %g2
	ldx	[%g5 + ITA_END], %g3

	wr	%g0, ASI_DMMU, %asi

	brz,a,pt %g1, 1f
	 mov	TLB_DEMAP_NUCLEUS | TLB_DEMAP_PAGE, %g4

	stxa	%g1, [%g0 + AA_DMMU_SCXR] %asi
	membar	#Sync
	mov	TLB_DEMAP_SECONDARY | TLB_DEMAP_PAGE, %g4

1:	set	PAGE_SIZE, %g5

2:	or	%g4, %g2, %g4
	stxa	%g0, [%g4] ASI_DMMU_DEMAP
	stxa	%g0, [%g4] ASI_IMMU_DEMAP

	add	%g2, %g5, %g2
	cmp	%g2, %g3
	bne,a,pt %xcc, 2b
	 nop

	brz,a,pt %g1, 3f
	 nop
	stxa	%g0, [%g0 + AA_DMMU_SCXR] %asi

3:	membar	#Sync

	IPI_WAIT(%g5, %g1, %g2)
	retry
END(tl_ipi_tlb_range_demap)

/*
 * Demap an entire context from the dtlb and itlb.
 */
ENTRY(tl_ipi_tlb_context_demap)
	ldx	[%g5 + ITA_CTX], %g1

	mov	AA_DMMU_SCXR, %g2
	stxa	%g1, [%g2] ASI_DMMU
	membar	#Sync

	mov	TLB_DEMAP_SECONDARY | TLB_DEMAP_CONTEXT, %g3
	stxa	%g0, [%g3] ASI_DMMU_DEMAP
	stxa	%g0, [%g3] ASI_IMMU_DEMAP

	stxa	%g0, [%g2] ASI_DMMU
	membar	#Sync

	IPI_WAIT(%g5, %g1, %g2)
	retry
END(tl_ipi_tlb_context_demap)
