/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI: locore.s,v 1.36.2.15 1999/08/23 22:34:41 cp Exp
 */
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
 *
 * $FreeBSD$
 */

#include "opt_ddb.h"

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/ktr.h>
#include <machine/pstate.h>
#include <machine/trap.h>
#include <machine/tstate.h>
#include <machine/wstate.h>

#include "assym.s"

/*
 * Macros for spilling and filling live windows.
 *
 * NOTE: These macros use exactly 16 instructions, and it is assumed that the
 * handler will not use more than 24 instructions total, to leave room for
 * resume vectors which occupy the last 8 instructions.
 */

#define	SPILL(storer, base, size, asi) \
	storer	%l0, [base + (0 * size)] asi ; \
	storer	%l1, [base + (1 * size)] asi ; \
	storer	%l2, [base + (2 * size)] asi ; \
	storer	%l3, [base + (3 * size)] asi ; \
	storer	%l4, [base + (4 * size)] asi ; \
	storer	%l5, [base + (5 * size)] asi ; \
	storer	%l6, [base + (6 * size)] asi ; \
	storer	%l7, [base + (7 * size)] asi ; \
	storer	%i0, [base + (8 * size)] asi ; \
	storer	%i1, [base + (9 * size)] asi ; \
	storer	%i2, [base + (10 * size)] asi ; \
	storer	%i3, [base + (11 * size)] asi ; \
	storer	%i4, [base + (12 * size)] asi ; \
	storer	%i5, [base + (13 * size)] asi ; \
	storer	%i6, [base + (14 * size)] asi ; \
	storer	%i7, [base + (15 * size)] asi

#define	FILL(loader, base, size, asi) \
	loader	[base + (0 * size)] asi, %l0 ; \
	loader	[base + (1 * size)] asi, %l1 ; \
	loader	[base + (2 * size)] asi, %l2 ; \
	loader	[base + (3 * size)] asi, %l3 ; \
	loader	[base + (4 * size)] asi, %l4 ; \
	loader	[base + (5 * size)] asi, %l5 ; \
	loader	[base + (6 * size)] asi, %l6 ; \
	loader	[base + (7 * size)] asi, %l7 ; \
	loader	[base + (8 * size)] asi, %i0 ; \
	loader	[base + (9 * size)] asi, %i1 ; \
	loader	[base + (10 * size)] asi, %i2 ; \
	loader	[base + (11 * size)] asi, %i3 ; \
	loader	[base + (12 * size)] asi, %i4 ; \
	loader	[base + (13 * size)] asi, %i5 ; \
	loader	[base + (14 * size)] asi, %i6 ; \
	loader	[base + (15 * size)] asi, %i7

#define	ERRATUM50(reg)	mov reg, reg

/*
 * Magic to resume from a spill or fill trap.  If we get an alignment or an
 * mmu fault during a spill or a fill, this macro will detect the fault and
 * resume at a set instruction offset in the trap handler, which will try to
 * get help.
 *
 * To check if the previous trap was a spill/fill we convert the the trapped
 * pc to a trap type and verify that it is in the range of spill/fill vectors.
 * The spill/fill vectors are types 0x80-0xff and 0x280-0x2ff, masking off the
 * tl bit allows us to detect both ranges with one test.
 *
 * This is:
 *	(((%tpc - %tba) >> 5) & ~0x200) >= 0x80 && <= 0xff
 *
 * Values outside of the trap table will produce negative or large positive
 * results.
 *
 * To calculate the new pc we take advantage of the xor feature of wrpr.
 * Forcing all the low bits of the trapped pc on we can produce any offset
 * into the spill/fill vector.  The size of a spill/fill trap vector is 0x80.
 *
 *	0x7f ^ 0x1f == 0x60
 *	0x1f == (0x80 - 0x60) - 1
 *
 * Which are the offset and xor value used to resume from mmu faults.
 */

/*
 * If a spill/fill trap is not detected this macro will branch to the label l1.
 * Otherwise the caller should do any necesary cleanup and execute a done.
 */
#define	RESUME_SPILLFILL_MAGIC(r1, r2, xor, l1) \
	rdpr	%tpc, r1 ; \
	ERRATUM50(r1) ; \
	rdpr	%tba, r2 ; \
	sub	r1, r2, r2 ; \
	srlx	r2, 5, r2 ; \
	andn	r2, 0x200, r2 ; \
	sub	r2, 0x80, r2 ; \
	brlz	r2, l1 ; \
	 sub	r2, 0x7f, r2 ; \
	brgz	r2, l1 ; \
	 or	r1, 0x7f, r1 ; \
	wrpr	r1, xor, %tnpc ; \

#define	RSF_XOR(off)	((0x80 - off) - 1)

/*
 * Instruction offsets in spill and fill trap handlers for handling certain
 * nested traps, and corresponding xor constants for wrpr.
 */
#define	RSF_OFF_MMU	0x60
#define	RSF_OFF_ALIGN	0x70

#define	RSF_MMU		RSF_XOR(RSF_OFF_MMU)
#define	RSF_ALIGN	RSF_XOR(RSF_OFF_ALIGN)

/*
 * Constant to add to %tnpc when taking a fill trap just before returning to
 * user mode.  The instruction sequence looks like restore, wrpr, retry; we
 * want to skip over the wrpr and retry and execute code to call back into the
 * kernel.  It is useful to add tracing between these instructions, which would
 * change the size of the sequence, so we demark with labels and subtract.
 */
#define	RSF_FILL_INC	tl0_ret_fill_end - tl0_ret_fill

/*
 * Retry a spill or fill with a different wstate due to an alignment fault.
 * We may just be using the wrong stack offset.
 */
#define	RSF_ALIGN_RETRY(ws) \
	wrpr	%g0, (ws), %wstate ; \
	retry ; \
	.align	16

/*
 * Generate a T_SPILL or T_FILL trap if the window operation fails.
 */
#define	RSF_TRAP(type) \
	b	%xcc, tl0_sftrap ; \
	 mov	type, %g2 ; \
	.align	16

/*
 * Game over if the window operation fails.
 */
#define	RSF_FATAL(type) \
	sir	type ; \
	.align	16

/*
 * Magic to resume from a failed fill a few instructions after the corrsponding
 * restore.  This is used on return from the kernel to usermode.
 */
#define	RSF_FILL_MAGIC \
	rdpr	%tnpc, %g1 ; \
	add	%g1, RSF_FILL_INC, %g1 ; \
	wrpr	%g1, 0, %tnpc ; \
	done ; \
	.align	16

/*
 * Spill to the pcb if a spill to the user stack in kernel mode fails.
 */
#define	RSF_SPILL_TOPCB \
	b,a	%xcc, tl1_spill_topcb ; \
	 nop ; \
	.align	16

DATA(intrnames)
	.asciz	"foo"
DATA(eintrnames)

DATA(intrcnt)
	.long	0
DATA(eintrcnt)

/*
 * Trap table and associated macros
 *
 * Due to its size a trap table is an inherently hard thing to represent in
 * code in a clean way.  There are approximately 1024 vectors, of 8 or 32
 * instructions each, many of which are identical.  The way that this is
 * layed out is the instructions (8 or 32) for the actual trap vector appear
 * as an AS macro.  In general this code branches to tl0_trap or tl1_trap,
 * but if not supporting code can be placed just after the definition of the
 * macro.  The macros are then instantiated in a different section (.trap),
 * which is setup to be placed by the linker at the beginning of .text, and the
 * code around the macros is moved to the end of trap table.  In this way the
 * code that must be sequential in memory can be split up, and located near
 * its supporting code so that it is easier to follow.
 */

	/*
	 * Clean window traps occur when %cleanwin is zero to ensure that data
	 * is not leaked between address spaces in registers.
	 */
	.macro	clean_window
	clr	%o0
	clr	%o1
	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5
	clr	%o6
	clr	%o7
	clr	%l0
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	rdpr	%cleanwin, %l7
	inc	%l7
	wrpr	%l7, 0, %cleanwin
	clr	%l7
	retry
	.align	128
	.endm

	/*
	 * Stack fixups for entry from user mode.  We are still running on the
	 * user stack, and with its live registers, so we must save soon.  We
	 * are on alternate globals so we do have some registers.  Set the
	 * transitional window state, save, and call a routine to get onto
	 * the kernel stack.  If the save traps we attempt to spill a window
	 * to the user stack.  If this fails, we spill the window to the pcb
	 * and continue.
	 *
	 * NOTE: Must be called with alternate globals and clobbers %g1.
	 */

	.macro	tl0_kstack
	rdpr	%wstate, %g1
	wrpr	%g1, WSTATE_TRANSITION, %wstate
	save
	call	tl0_kstack_fixup
	 rdpr	%canrestore, %o0
	.endm

	.macro	tl0_setup	type
	tl0_kstack
	rdpr	%pil, %o2
	b	%xcc, tl0_trap
	 mov	\type, %o0
	.endm

/*
 * Setup the kernel stack and split the register windows when faulting from
 * user space.
 * %canrestore is passed in %o0 and %wstate in (alternate) %g1.
 */
ENTRY(tl0_kstack_fixup)
	mov	%g1, %o3
	and	%o3, WSTATE_MASK, %o1
	sllx	%o0, WSTATE_USERSHIFT, %o1
	wrpr	%o1, 0, %wstate
	wrpr	%o0, 0, %otherwin
	wrpr	%g0, 0, %canrestore
	ldx	[PCPU(CURPCB)], %o0
	set	UPAGES * PAGE_SIZE - SPOFF - CCFSZ, %o1
	retl
	 add	%o0, %o1, %sp
END(tl0_kstack_fixup)

	/*
	 * Generic trap type.  Call trap() with the specified type.
	 */
	.macro	tl0_gen		type
	tl0_setup \type
	.align	32
	.endm

	.macro	tl0_wide	type
	tl0_setup \type
	.align	128
	.endm

	/*
	 * This is used to suck up the massive swaths of reserved trap types.
	 * Generates count "reserved" trap vectors.
	 */
	.macro	tl0_reserved	count
	.rept	\count
	tl0_gen	T_RESERVED
	.endr
	.endm

	/*
	 * NOTE: we cannot use mmu globals here because tl0_kstack may cause
	 * an mmu fault.
	 */
	.macro	tl0_data_excptn
	wrpr	%g0, PSTATE_ALT, %pstate
	wr	%g0, ASI_DMMU, %asi
	ldxa	[%g0 + AA_DMMU_SFAR] %asi, %g3
	ldxa	[%g0 + AA_DMMU_SFSR] %asi, %g4
	ldxa	[%g0 + AA_DMMU_TAR] %asi, %g5
	b	%xcc, tl0_sfsr_trap
	 mov	T_DATA_EXCPTN, %g2
	.align	32
	.endm

	.macro	tl0_align
	wr	%g0, ASI_DMMU, %asi
	ldxa	[%g0 + AA_DMMU_SFAR] %asi, %g3
	ldxa	[%g0 + AA_DMMU_SFSR] %asi, %g4
	b	%xcc, tl0_sfsr_trap
	 mov	T_ALIGN, %g2
	.align	32
	.endm

ENTRY(tl0_sfsr_trap)
	/*
	 * Clear the sfsr.
	 */
	stxa	%g0, [%g0 + AA_DMMU_SFSR] %asi
	membar	#Sync

	/*
	 * Get onto the kernel stack, save the mmu registers, and call
	 * common code.
	 */
	tl0_kstack
	sub	%sp, MF_SIZEOF, %sp
	stx	%g3, [%sp + SPOFF + CCFSZ + MF_SFAR]
	stx	%g4, [%sp + SPOFF + CCFSZ + MF_SFSR]
	stx	%g5, [%sp + SPOFF + CCFSZ + MF_TAR]
	rdpr	%pil, %o2
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl0_trap
	 mov	%g2, %o0
END(tl0_sfsr_trap)

	.macro	tl0_intr level, mask
	tl0_kstack
	set	\mask, %o2
	b	%xcc, tl0_intr_call_trap
	 mov	\level, %o1
	.align	32
	.endm

/*
 * Actually call tl0_trap, and do some work that cannot be done in tl0_intr
 * because of space constraints.
 */
ENTRY(tl0_intr_call_trap)
	wr	%o2, 0, %asr21
	rdpr	%pil, %o2
	wrpr	%g0, %o1, %pil
	b	%xcc, tl0_trap
	 mov	T_INTR, %o0
END(tl0_intr_call_trap)

#define	INTR(level, traplvl)						\
	tl ## traplvl ## _intr	level, 1 << level

#define	TICK(traplvl) \
	tl ## traplvl ## _intr	PIL_TICK, 1

#define	INTR_LEVEL(tl)							\
	INTR(1, tl) ;							\
	INTR(2, tl) ;							\
	INTR(3, tl) ;							\
	INTR(4, tl) ;							\
	INTR(5, tl) ;							\
	INTR(6, tl) ;							\
	INTR(7, tl) ;							\
	INTR(8, tl) ;							\
	INTR(9, tl) ;							\
	INTR(10, tl) ;							\
	INTR(11, tl) ;							\
	INTR(12, tl) ;							\
	INTR(13, tl) ;							\
	TICK(tl) ;							\
	INTR(15, tl) ;

	.macro	tl0_intr_level
	INTR_LEVEL(0)
	.endm

	.macro	tl0_intr_vector
	b,a	intr_enqueue
	.align	32
	.endm

	.macro	tl0_immu_miss
	/*
	 * Force kernel store order.
	 */
	wrpr	%g0, PSTATE_MMU, %pstate

	/*
	 * Extract the 8KB pointer and convert to an index.
	 */
	ldxa	[%g0] ASI_IMMU_TSB_8KB_PTR_REG, %g1	
	srax	%g1, TTE_SHIFT, %g1

	/*
	 * Compute the stte address in the primary used tsb.
	 */
	and	%g1, (1 << TSB_PRIMARY_MASK_WIDTH) - 1, %g2
	sllx	%g2, TSB_PRIMARY_STTE_SHIFT, %g2
	setx	TSB_USER_MIN_ADDRESS, %g4, %g3
	add	%g2, %g3, %g2

	/*
	 * Preload the tte tag target.
	 */
	ldxa	[%g0] ASI_IMMU_TAG_TARGET_REG, %g3

	/*
	 * Preload tte data bits to check inside the bucket loop.
	 */
	and	%g1, TD_VA_LOW_MASK >> TD_VA_LOW_SHIFT, %g4
	sllx	%g4, TD_VA_LOW_SHIFT, %g4
	or	%g4, TD_EXEC, %g4

	/*
	 * Preload mask for tte data check.
	 */
	setx	TD_VA_LOW_MASK, %g5, %g1
	or	%g1, TD_EXEC, %g1

	/*
	 * Loop over the sttes in this bucket
	 */

	/*
	 * Load the tte.
	 */
1:	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g6

	/*
	 * Compare the tag.
	 */
	cmp	%g6, %g3
	bne,pn	%xcc, 2f

	/*
	 * Compare the data.
	 */
	 xor	%g7, %g4, %g5
	brgez,pn %g7, 2f
	 andcc	%g5, %g1, %g0
	bnz,pn	%xcc, 2f

	/*
	 * We matched a tte, load the tlb.
	 */

	/*
	 * Set the reference bit, if it's currently clear.
	 */
	 andcc	%g7, TD_REF, %g0
	bz,a,pn	%xcc, tl0_immu_miss_set_ref
	 nop

	/*
	 * Load the tte data into the tlb and retry the instruction.
	 */
	stxa	%g7, [%g0] ASI_ITLB_DATA_IN_REG
	retry

	/*
	 * Check the low bits to see if we've finished the bucket.
	 */
2:	add	%g2, STTE_SIZEOF, %g2
	andcc	%g2, TSB_PRIMARY_STTE_MASK, %g0
	bnz	%xcc, 1b
	 nop
	b,a	%xcc, tl0_immu_miss_trap
	.align	128
	.endm

ENTRY(tl0_immu_miss_set_ref)
	/*
	 * Set the reference bit.
	 */
	add	%g2, TTE_DATA, %g2
1:	or	%g7, TD_REF, %g1
	casxa	[%g2] ASI_N, %g7, %g1
	cmp	%g1, %g7
	bne,a,pn %xcc, 1b
	 mov	%g1, %g7

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl0_immu_miss: set ref"
	    , %g2, %g3, %g4, 7, 8, 9)
9:
#endif

	/*
	 * May have become invalid, in which case start over.
	 */
	brgez,pn %g1, 2f
	 nop

	/*
	 * Load the tte data into the tlb and retry the instruction.
	 */
	stxa	%g1, [%g0] ASI_ITLB_DATA_IN_REG
2:	retry
END(tl0_immu_miss_set_ref)

ENTRY(tl0_immu_miss_trap)
	/*
	 * Switch to alternate globals.
	 */
	wrpr	%g0, PSTATE_ALT, %pstate

	/*
	 * Load the tar, sfar and sfsr aren't valid.
	 */
	wr	%g0, ASI_IMMU, %asi
	ldxa	[%g0 + AA_IMMU_TAR] %asi, %g2

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl0_immu_miss: trap sp=%#lx tar=%#lx"
	    , %g3, %g4, %g5, 7, 8, 9)
	stx	%sp, [%g3 + KTR_PARM1]
	stx	%g2, [%g3 + KTR_PARM2]
9:
#endif

	/*
	 * Save the mmu registers on the stack, and call common trap code.
	 */
	tl0_kstack
	sub	%sp, MF_SIZEOF, %sp
	stx	%g2, [%sp + SPOFF + CCFSZ + MF_TAR]
	rdpr	%pil, %o2
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl0_trap
	 mov	T_IMMU_MISS, %o0
END(tl0_immu_miss_trap)

	.macro	dmmu_miss_user
	/*
	 * Extract the 8KB pointer and convert to an index.
	 */
	ldxa	[%g0] ASI_DMMU_TSB_8KB_PTR_REG, %g1	
	srax	%g1, TTE_SHIFT, %g1

	/*
	 * Compute the stte address in the primary used tsb.
	 */
	and	%g1, (1 << TSB_PRIMARY_MASK_WIDTH) - 1, %g2
	sllx	%g2, TSB_PRIMARY_STTE_SHIFT, %g2
	setx	TSB_USER_MIN_ADDRESS, %g4, %g3
	add	%g2, %g3, %g2

	/*
	 * Preload the tte tag target.
	 */
	ldxa	[%g0] ASI_DMMU_TAG_TARGET_REG, %g3

	/*
	 * Preload tte data bits to check inside the bucket loop.
	 */
	and	%g1, TD_VA_LOW_MASK >> TD_VA_LOW_SHIFT, %g4
	sllx	%g4, TD_VA_LOW_SHIFT, %g4

	/*
	 * Preload mask for tte data check.
	 */
	setx	TD_VA_LOW_MASK, %g5, %g1

	/*
	 * Loop over the sttes in this bucket
	 */

	/*
	 * Load the tte.
	 */
1:	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g6

	/*
	 * Compare the tag.
	 */
	cmp	%g6, %g3
	bne,pn	%xcc, 2f

	/*
	 * Compare the data.
	 */
	 xor	%g7, %g4, %g5
	brgez,pn %g7, 2f
	 andcc	%g5, %g1, %g0
	bnz,pn	%xcc, 2f

	/*
	 * We matched a tte, load the tlb.
	 */

	/*
	 * Set the reference bit, if it's currently clear.
	 */
	 andcc	%g7, TD_REF, %g0
	bz,a,pn	%xcc, dmmu_miss_user_set_ref
	 nop

	/*
	 * Load the tte data into the tlb and retry the instruction.
	 */
	stxa	%g7, [%g0] ASI_DTLB_DATA_IN_REG
	retry

	/*
	 * Check the low bits to see if we've finished the bucket.
	 */
2:	add	%g2, STTE_SIZEOF, %g2
	andcc	%g2, TSB_PRIMARY_STTE_MASK, %g0
	bnz	%xcc, 1b
	 nop
	.endm

ENTRY(dmmu_miss_user_set_ref)
	/*
	 * Set the reference bit.
	 */
	add	%g2, TTE_DATA, %g2
1:	or	%g7, TD_REF, %g1
	casxa	[%g2] ASI_N, %g7, %g1
	cmp	%g1, %g7
	bne,a,pn %xcc, 1b
	 mov	%g1, %g7

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl0_dmmu_miss: set ref"
	    , %g2, %g3, %g4, 7, 8, 9)
9:
#endif

	/*
	 * May have become invalid, in which case start over.
	 */
	brgez,pn %g1, 2f
	 nop

	/*
	 * Load the tte data into the tlb and retry the instruction.
	 */
	stxa	%g1, [%g0] ASI_DTLB_DATA_IN_REG
2:	retry
END(dmmu_miss_user_set_ref)

	.macro	tl0_dmmu_miss
	/*
	 * Force kernel store order.
	 */
	wrpr	%g0, PSTATE_MMU, %pstate

	/*
	 * Try a fast inline lookup of the primary tsb.
	 */
	dmmu_miss_user

	/*
	 * Not in primary tsb, call c code.  Nothing else fits inline.
	 */
	b,a	tl0_dmmu_miss_trap
	.align	128
	.endm

ENTRY(tl0_dmmu_miss_trap)
	/*
	 * Switch to alternate globals.
	 */
	wrpr	%g0, PSTATE_ALT, %pstate

	/*
	 * Load the tar, sfar and sfsr aren't valid.
	 */
	wr	%g0, ASI_DMMU, %asi
	ldxa	[%g0 + AA_DMMU_TAR] %asi, %g2

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl0_dmmu_miss: trap sp=%#lx tar=%#lx"
	    , %g3, %g4, %g5, 7, 8, 9)
	stx	%sp, [%g3 + KTR_PARM1]
	stx	%g2, [%g3 + KTR_PARM2]
9:
#endif

	/*
	 * Save the mmu registers on the stack and call common trap code.
	 */
	tl0_kstack
	sub	%sp, MF_SIZEOF, %sp
	stx	%g2, [%sp + SPOFF + CCFSZ + MF_TAR]
	rdpr	%pil, %o2
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl0_trap
	 mov	T_DMMU_MISS, %o0
END(tl0_dmmu_miss_trap)

	.macro	tl0_dmmu_prot
	/*
	 * Switch to alternate globals.
	 */
	wrpr	%g0, PSTATE_ALT, %pstate

	/*
	 * Load the tar, sfar and sfsr.
	 */
	wr	%g0, ASI_DMMU, %asi
	ldxa	[%g0 + AA_DMMU_SFAR] %asi, %g2
	ldxa	[%g0 + AA_DMMU_SFSR] %asi, %g3
	ldxa	[%g0 + AA_DMMU_TAR] %asi, %g4
	stxa	%g0, [%g0 + AA_DMMU_SFSR] %asi
	membar	#Sync

	/*
	 * Save the mmu registers on the stack, switch to alternate globals,
	 * and call common trap code.
	 */
	tl0_kstack
	sub	%sp, MF_SIZEOF, %sp
	stx	%g2, [%sp + SPOFF + CCFSZ + MF_TAR]
	stx	%g3, [%sp + SPOFF + CCFSZ + MF_SFAR]
	stx	%g4, [%sp + SPOFF + CCFSZ + MF_SFSR]
	rdpr	%pil, %o2
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl0_trap
	 mov	T_DMMU_PROT, %o0
	.align	128
	.endm

	.macro	tl0_spill_0_n
	andcc	%sp, 1, %g0
	bz,pn	%xcc, 2f
	 wr	%g0, ASI_AIUP, %asi
1:	SPILL(stxa, %sp + SPOFF, 8, %asi)
	saved
	wrpr	%g0, WSTATE_ASSUME64, %wstate
	retry
	.align	32
	RSF_TRAP(T_SPILL)
	RSF_TRAP(T_SPILL)
	.endm

	.macro	tl0_spill_1_n
	andcc	%sp, 1, %g0
	bnz	%xcc, 1b
	 wr	%g0, ASI_AIUP, %asi
2:	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	SPILL(stwa, %sp, 4, %asi)
	saved
	wrpr	%g0, WSTATE_ASSUME32, %wstate
	retry
	.align	32
	RSF_TRAP(T_SPILL)
	RSF_TRAP(T_SPILL)
	.endm

	.macro	tl0_spill_2_n
	wr	%g0, ASI_AIUP, %asi
	SPILL(stxa, %sp + SPOFF, 8, %asi)
	saved
	retry
	.align	32
	RSF_ALIGN_RETRY(WSTATE_TEST32)
	RSF_TRAP(T_SPILL)
	.endm

	.macro	tl0_spill_3_n
	wr	%g0, ASI_AIUP, %asi
	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	SPILL(stwa, %sp, 4, %asi)
	saved
	retry
	.align	32
	RSF_ALIGN_RETRY(WSTATE_TEST64)
	RSF_TRAP(T_SPILL)
	.endm

	.macro	tl0_fill_0_n
	andcc	%sp, 1, %g0
	bz,pn	%xcc, 2f
	 wr	%g0, ASI_AIUP, %asi
1:	FILL(ldxa, %sp + SPOFF, 8, %asi)
	restored
	wrpr	%g0, WSTATE_ASSUME64, %wstate
	retry
	.align	32
	RSF_TRAP(T_FILL)
	RSF_TRAP(T_FILL)
	.endm

	.macro	tl0_fill_1_n
	andcc	%sp, 1, %g0
	bnz	%xcc, 1b
	 wr	%g0, ASI_AIUP, %asi
2:	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	FILL(lduwa, %sp, 4, %asi)
	restored
	wrpr	%g0, WSTATE_ASSUME32, %wstate
	retry
	.align	32
	RSF_TRAP(T_FILL)
	RSF_TRAP(T_FILL)
	.endm

	.macro	tl0_fill_2_n
	wr	%g0, ASI_AIUP, %asi
	FILL(ldxa, %sp + SPOFF, 8, %asi)
	restored
	retry
	.align	32
	RSF_ALIGN_RETRY(WSTATE_TEST32)
	RSF_TRAP(T_FILL)
	.endm

	.macro	tl0_fill_3_n
	wr	%g0, ASI_AIUP, %asi
	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	FILL(lduwa, %sp, 4, %asi)
	restored
	retry
	.align	32
	RSF_ALIGN_RETRY(WSTATE_TEST64)
	RSF_TRAP(T_FILL)
	.endm

ENTRY(tl0_sftrap)
	rdpr	%tstate, %g1
	and	%g1, TSTATE_CWP_MASK, %g1
	wrpr	%g1, 0, %cwp
	tl0_kstack
	rdpr	%pil, %o2
	b	%xcc, tl0_trap
	 mov	%g2, %o0
END(tl0_sftrap)

	.macro	tl0_spill_bad	count
	.rept	\count
	tl0_wide T_SPILL
	.endr
	.endm

	.macro	tl0_fill_bad	count
	.rept	\count
	tl0_wide T_FILL
	.endr
	.endm

	.macro	tl0_soft	count
	.rept	\count
	tl0_gen	T_SOFT
	.endr
	.endm

	.macro	tl1_kstack
	save	%sp, -CCFSZ, %sp
	.endm

	.macro	tl1_setup	type
	tl1_kstack
	mov	\type | T_KERNEL, %o0
	b	%xcc, tl1_trap
	 rdpr	%pil, %o2
	.endm

	.macro	tl1_gen		type
	tl1_setup \type
	.align	32
	.endm

	.macro	tl1_wide	type
	tl1_setup \type
	.align	128
	.endm

	.macro	tl1_reserved	count
	.rept	\count
	tl1_gen	T_RESERVED
	.endr
	.endm

	.macro	tl1_insn_excptn
	tl1_kstack
	wrpr	%g0, PSTATE_ALT, %pstate
	rdpr	%pil, %o2
	b	%xcc, tl1_trap
	 mov	T_INSN_EXCPTN | T_KERNEL, %o0
	.align	32
	.endm

	.macro	tl1_data_excptn
	b,a	%xcc, tl1_data_exceptn_trap
	 nop
	.align	32
	.endm

ENTRY(tl1_data_exceptn_trap)
	wr	%g0, ASI_DMMU, %asi
	RESUME_SPILLFILL_MAGIC(%g1, %g2, RSF_MMU, 1f)
	stxa	%g0, [%g0 + AA_DMMU_SFSR] %asi
	done

1:	wrpr	%g0, PSTATE_ALT, %pstate
	b	%xcc, tl1_sfsr_trap
	 mov	T_DATA_EXCPTN | T_KERNEL, %g1
END(tl1_data_exceptn)

	/*
	 * NOTE: We switch to mmu globals here, to avoid needing to save
	 * alternates, which may be live.
	 */
	.macro	tl1_align
	b	%xcc, tl1_align_trap
	 wrpr	%g0, PSTATE_MMU, %pstate
	.align	32
	.endm

ENTRY(tl1_align_trap)
	wr	%g0, ASI_DMMU, %asi
	RESUME_SPILLFILL_MAGIC(%g1, %g2, RSF_ALIGN, 1f)
	stxa	%g0, [%g0 + AA_DMMU_SFSR] %asi
	done

1:	wrpr	%g0, PSTATE_ALT, %pstate
	b	%xcc, tl1_sfsr_trap
	 mov	T_ALIGN | T_KERNEL, %g1
END(tl1_align_trap)

ENTRY(tl1_sfsr_trap)
!	wr	%g0, ASI_DMMU, %asi
	ldxa	[%g0 + AA_DMMU_SFAR] %asi, %g2
	ldxa	[%g0 + AA_DMMU_SFSR] %asi, %g3
	stxa	%g0, [%g0 + AA_DMMU_SFSR] %asi
	membar	#Sync

	tl1_kstack
	sub	%sp, MF_SIZEOF, %sp
	stx	%g2, [%sp + SPOFF + CCFSZ + MF_SFAR]
	stx	%g3, [%sp + SPOFF + CCFSZ + MF_SFSR]
	rdpr	%pil, %o2
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl1_trap
	 mov	%g1, %o0
END(tl1_align_trap)

	.macro	tl1_intr level, mask, type
	tl1_kstack
	rdpr	%pil, %o2
	wrpr	%g0, \level, %pil
	set	\mask, %o3
	wr	%o3, 0, %asr21
	mov	T_INTR | T_KERNEL, %o0
	b	%xcc, tl1_trap
	 mov	\level, %o1
	.align	32
	.endm

	.macro	tl1_intr_level
	INTR_LEVEL(1)
	.endm

	.macro	tl1_intr_vector
	b,a	intr_enqueue
	.align	32
	.endm

ENTRY(intr_enqueue)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "intr_enqueue: p=%p (%s) tl=%#lx pc=%#lx sp=%#lx"
	   , %g1, %g2, %g3, 7, 8, 9)
	ldx	[PCPU(CURPROC)], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	add	%g2, P_COMM, %g2
	stx	%g2, [%g1 + KTR_PARM2]
	rdpr	%tl, %g2
	stx	%g2, [%g1 + KTR_PARM3]
	rdpr	%tpc, %g2
	stx	%g2, [%g1 + KTR_PARM4]
	stx	%sp, [%g1 + KTR_PARM5]
9:
#endif

	/*
	 * Find the head of the queue and advance it.
	 */
	ldx	[PCPU(IQ)], %g1
	ldx	[%g1 + IQ_HEAD], %g2
	add	%g2, 1, %g3
	and	%g3, IQ_MASK, %g3
	stx	%g3, [%g1 + IQ_HEAD]

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "intr_enqueue: cpu=%d head=%d tail=%d iqe=%d"
	    , %g4, %g5, %g6, 7, 8, 9)
	lduw	[PCPU(CPUID)], %g5
	stx	%g5, [%g4 + KTR_PARM1]
	stx	%g3, [%g4 + KTR_PARM2]
	ldx	[%g1 + IQ_TAIL], %g5
	stx	%g5, [%g4 + KTR_PARM3]
	stx	%g2, [%g4 + KTR_PARM4]
9:
#endif

#ifdef INVARIANTS
	/*
	 * If the new head is the same as the tail, the next interrupt will
	 * overwrite unserviced packets.  This is bad.
	 */
	ldx	[%g1 + IQ_TAIL], %g4
	cmp	%g4, %g3
	be	%xcc, 3f
	 nop
#endif

	/*
	 * Load the interrupt packet from the hardware.
	 */
	wr	%g0, ASI_SDB_INTR_R, %asi
	ldxa	[%g0] ASI_INTR_RECEIVE, %g3
	ldxa	[%g0 + AA_SDB_INTR_D0] %asi, %g4
	ldxa	[%g0 + AA_SDB_INTR_D1] %asi, %g5
	ldxa	[%g0 + AA_SDB_INTR_D2] %asi, %g6
	stxa	%g0, [%g0] ASI_INTR_RECEIVE
	membar	#Sync

	/*
	 * Store the tag and first data word in the iqe.  These are always
	 * valid.
	 */
	sllx	%g2, IQE_SHIFT, %g2
	add	%g2, %g1, %g2
	stw	%g3, [%g2 + IQE_TAG]
	stx	%g4, [%g2 + IQE_VEC]

	/*
	 * Find the interrupt vector associated with this source.
	 */
	ldx	[PCPU(IVT)], %g3
	sllx	%g4, IV_SHIFT, %g4

	/*
	 * If the 2nd data word, the function, is zero the actual function
	 * and argument are in the interrupt vector table, so retrieve them.
	 * The function is used as a lock on the vector data.  If it can be
	 * read atomically as non-zero, the argument and priority are valid.
	 * Otherwise this is either a true stray interrupt, or someone is
	 * trying to deregister the source as we speak.  In either case,
	 * bail and log a stray.
	 */
	brnz,pn %g5, 1f
	 add	%g3, %g4, %g3
	casxa	[%g3] ASI_N, %g0, %g5
	brz,pn	%g5, 2f
	 ldx	[%g3 + IV_ARG], %g6

	/*
	 * Save the priority and the two remaining data words in the iqe.
	 */
1:	lduw	[%g3 + IV_PRI], %g4
	stw	%g4, [%g2 + IQE_PRI]
	stx	%g5, [%g2 + IQE_FUNC]
	stx	%g6, [%g2 + IQE_ARG]

	/*
	 * Trigger a softint at the level indicated by the priority.
	 */
	mov	1, %g3
	sllx	%g3, %g4, %g3
	wr	%g3, 0, %asr20

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "intr_enqueue: tag=%#lx vec=%#lx pri=%d func=%p arg=%p"
	    , %g1, %g3, %g4, 7, 8, 9)
	lduw	[%g2 + IQE_TAG], %g3
	stx	%g3, [%g1 + KTR_PARM1]
	ldx	[%g2 + IQE_VEC], %g3
	stx	%g3, [%g1 + KTR_PARM2]
	lduw	[%g2 + IQE_PRI], %g3
	stx	%g3, [%g1 + KTR_PARM3]
	stx	%g5, [%g1 + KTR_PARM4]
	stx	%g6, [%g1 + KTR_PARM5]
9:
#endif

	retry

	/*
	 * Either this is a true stray interrupt, or an interrupt occured
	 * while the source was being deregistered.  In either case, just
	 * log the stray and return.  XXX
	 */
2:	DEBUGGER()

#ifdef INVARIANTS
	/*
	 * The interrupt queue is about to overflow.  We are in big trouble.
	 */
3:	DEBUGGER()
#endif
END(intr_enqueue)

	.macro	tl1_immu_miss
	wrpr	%g0, PSTATE_ALT, %pstate
	tl1_kstack
	rdpr	%pil, %o2
	b	%xcc, tl1_trap
	 mov	T_IMMU_MISS | T_KERNEL, %o0
	.align	128
	.endm

	.macro	tl1_dmmu_miss
	/*
	 * Load the target tte tag, and extract the context.  If the context
	 * is non-zero handle as user space access.  In either case, load the
	 * tsb 8k pointer.
	 */
	ldxa	[%g0] ASI_DMMU_TAG_TARGET_REG, %g1
	srlx	%g1, TT_CTX_SHIFT, %g2
	brnz,pn	%g2, tl1_dmmu_miss_user
	 ldxa	[%g0] ASI_DMMU_TSB_8KB_PTR_REG, %g2

	/*
	 * Convert the tte pointer to an stte pointer, and add extra bits to
	 * accomodate for large tsb.
	 */
	sllx	%g2, STTE_SHIFT - TTE_SHIFT, %g2
#ifdef notyet
	mov	AA_DMMU_TAR, %g3
	ldxa	[%g3] ASI_DMMU, %g3
	srlx	%g3, TSB_1M_STTE_SHIFT, %g3
	and	%g3, TSB_KERNEL_MASK >> TSB_1M_STTE_SHIFT, %g3
	sllx	%g3, TSB_1M_STTE_SHIFT, %g3
	add	%g2, %g3, %g2
#endif

	/*
	 * Load the tte, check that it's valid and that the tags match.
	 */
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4 /*, %g5 */
	brgez,pn %g5, 2f
	 cmp	%g4, %g1
	bne	%xcc, 2f
	 EMPTY

	/*
	 * Set the refence bit, if its currently clear.
	 */
	andcc	%g5, TD_REF, %g0
	bnz	%xcc, 1f
	 or	%g5, TD_REF, %g1
	stx	%g1, [%g2 + ST_TTE + TTE_DATA]

	/*
	 * Load the tte data into the TLB and retry the instruction.
	 */
1:	stxa	%g5, [%g0] ASI_DTLB_DATA_IN_REG
	retry

	/*
	 * Switch to alternate globals.
	 */
2:	wrpr	%g0, PSTATE_ALT, %pstate

	wr	%g0, ASI_DMMU, %asi
	ldxa	[%g0 + AA_DMMU_TAR] %asi, %g1

	tl1_kstack
	sub	%sp, MF_SIZEOF, %sp
	stx	%g1, [%sp + SPOFF + CCFSZ + MF_TAR]
	wrpr	%g0, PSTATE_ALT, %pstate
	rdpr	%pil, %o2
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl1_trap
	 mov	T_DMMU_MISS | T_KERNEL, %o0
	.align	128
	.endm

ENTRY(tl1_dmmu_miss_user)
	/*
	 * Try a fast inline lookup of the primary tsb.
	 */
	dmmu_miss_user

	/* Handle faults during window spill/fill. */
	RESUME_SPILLFILL_MAGIC(%g1, %g2, RSF_MMU, 1f)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl1_dmmu_miss_user: resume spillfill npc=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	rdpr	%tnpc, %g2
	stx	%g2, [%g1 + KTR_PARM1]
9:
#endif
	done
1:

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl1_dmmu_miss_user: trap", %g1, %g2, %g3, 7, 8, 9)
9:
#endif

	/*
	 * Switch to alternate globals.
	 */
	wrpr	%g0, PSTATE_ALT, %pstate

	wr	%g0, ASI_DMMU, %asi
	ldxa	[%g0 + AA_DMMU_TAR] %asi, %g1
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl1_dmmu_miss: trap sp=%#lx tar=%#lx"
	    , %g2, %g3, %g4, 7, 8, 9)
	stx	%sp, [%g2 + KTR_PARM1]
	stx	%g1, [%g2 + KTR_PARM2]
9:
#endif

	tl1_kstack
	sub	%sp, MF_SIZEOF, %sp
	stx	%g1, [%sp + SPOFF + CCFSZ + MF_TAR]
	rdpr	%pil, %o2
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl1_trap
	 mov	T_DMMU_MISS | T_KERNEL, %o0
END(tl1_dmmu_miss_user)

	.macro	tl1_dmmu_prot
	wr	%g0, ASI_DMMU, %asi
	RESUME_SPILLFILL_MAGIC(%g1, %g2, RSF_MMU, 1f)
	stxa	%g0, [%g0 + AA_DMMU_SFSR] %asi
	done

	/*
	 * Switch to alternate globals.
	 */
1:	wrpr	%g0, PSTATE_ALT, %pstate

	/*
	 * Load the sfar, sfsr and tar.  Clear the sfsr.
	 */
	ldxa	[%g0 + AA_DMMU_TAR] %asi, %g1
	ldxa	[%g0 + AA_DMMU_SFAR] %asi, %g2
	ldxa	[%g0 + AA_DMMU_SFSR] %asi, %g3
	stxa	%g0, [%g0 + AA_DMMU_SFSR] %asi
	membar	#Sync

	tl1_kstack
	sub	%sp, MF_SIZEOF, %sp
	stx	%g1, [%sp + SPOFF + CCFSZ + MF_TAR]
	stx	%g2, [%sp + SPOFF + CCFSZ + MF_SFAR]
	stx	%g3, [%sp + SPOFF + CCFSZ + MF_SFSR]
	rdpr	%pil, %o2
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl1_trap
	 mov	T_DMMU_PROT | T_KERNEL, %o0
	.align	128
	.endm

	.macro	tl1_spill_0_n
	SPILL(stx, %sp + SPOFF, 8, EMPTY)
	saved
	retry
	.align	32
	RSF_FATAL(T_SPILL)
	RSF_FATAL(T_SPILL)
	.endm

	.macro	tl1_spill_4_n
	andcc	%sp, 1, %g0
	bz,pn	%xcc, 2f
	 wr	%g0, ASI_AIUP, %asi
1:	SPILL(stxa, %sp + SPOFF, 8, %asi)
	saved
	retry
	.align	32
	RSF_SPILL_TOPCB
	RSF_SPILL_TOPCB
	.endm

	.macro	tl1_spill_5_n
	andcc	%sp, 1, %g0
	bnz	%xcc, 1b
	 wr	%g0, ASI_AIUP, %asi
2:	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	SPILL(stwa, %sp, 4, %asi)
	saved
	retry
	.align	32
	RSF_SPILL_TOPCB
	RSF_SPILL_TOPCB
	.endm

	.macro	tl1_spill_6_n
	wr	%g0, ASI_AIUP, %asi
	SPILL(stxa, %sp + SPOFF, 8, %asi)
	saved
	retry
	.align	32
	RSF_ALIGN_RETRY(WSTATE_TRANSITION | WSTATE_TEST32)
	RSF_SPILL_TOPCB
	.endm

	.macro	tl1_spill_7_n
	wr	%g0, ASI_AIUP, %asi
	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	SPILL(stwa, %sp, 4, %asi)
	saved
	retry
	.align	32
	RSF_ALIGN_RETRY(WSTATE_TRANSITION | WSTATE_TEST64)
	RSF_SPILL_TOPCB
	.endm

	.macro	tl1_spill_0_o
	andcc	%sp, 1, %g0
	bz,pn	%xcc, 2f
	 wr	%g0, ASI_AIUP, %asi
1:	SPILL(stxa, %sp + SPOFF, 8, %asi)
	saved
	wrpr	%g0, WSTATE_ASSUME64 << WSTATE_USERSHIFT, %wstate
	retry
	.align	32
	RSF_SPILL_TOPCB
	RSF_SPILL_TOPCB
	.endm

	.macro	tl1_spill_1_o
	andcc	%sp, 1, %g0
	bnz	%xcc, 1b
	 wr	%g0, ASI_AIUP, %asi
2:	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	SPILL(stwa, %sp, 4, %asi)
	saved
	wrpr	%g0, WSTATE_ASSUME32 << WSTATE_USERSHIFT, %wstate
	retry
	.align	32
	RSF_SPILL_TOPCB
	RSF_SPILL_TOPCB
	.endm

	.macro	tl1_spill_2_o
	wr	%g0, ASI_AIUP, %asi
	SPILL(stxa, %sp + SPOFF, 8, %asi)
	saved
	retry
	.align	32
	RSF_ALIGN_RETRY(WSTATE_TEST32 << WSTATE_USERSHIFT)
	RSF_SPILL_TOPCB
	.endm

	.macro	tl1_spill_3_o
	wr	%g0, ASI_AIUP, %asi
	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	SPILL(stwa, %sp, 4, %asi)
	saved
	retry
	.align	32
	RSF_ALIGN_RETRY(WSTATE_TEST64 << WSTATE_USERSHIFT)
	RSF_SPILL_TOPCB
	.endm

	.macro	tl1_fill_0_n
	FILL(ldx, %sp + SPOFF, 8, EMPTY)
	restored
	retry
	.align	32
	RSF_FATAL(T_FILL)
	RSF_FATAL(T_FILL)
	.endm

	.macro	tl1_fill_4_n
	andcc	%sp, 1, %g0
	bz,pn	%xcc, 2f
	 wr	%g0, ASI_AIUP, %asi
1:	FILL(ldxa, %sp + SPOFF, 8, %asi)
	restored
	retry
	.align 32
	RSF_FILL_MAGIC
	RSF_FILL_MAGIC
	.endm

	.macro	tl1_fill_5_n
	andcc	%sp, 1, %g0
	bnz,pn	%xcc, 1b
	 wr	%g0, ASI_AIUP, %asi
2:	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	FILL(lduwa, %sp, 4, %asi)
	restored
	retry
	.align 32
	RSF_FILL_MAGIC
	RSF_FILL_MAGIC
	.endm

	.macro	tl1_fill_6_n
	wr	%g0, ASI_AIUP, %asi
	FILL(ldxa, %sp + SPOFF, 8, %asi)
	restored
	retry
	.align 32
	RSF_ALIGN_RETRY(WSTATE_TEST32 | WSTATE_TRANSITION)
	RSF_FILL_MAGIC
	.endm

	.macro	tl1_fill_7_n
	wr	%g0, ASI_AIUP, %asi
	wrpr	%g0, PSTATE_ALT | PSTATE_AM, %pstate
	FILL(lduwa, %sp, 4, %asi)
	restored
	retry
	.align 32
	RSF_ALIGN_RETRY(WSTATE_TEST64 | WSTATE_TRANSITION)
	RSF_FILL_MAGIC
	.endm

/*
 * This is used to spill windows that are still occupied with user
 * data on kernel entry to the pcb.
 */
ENTRY(tl1_spill_topcb)
	wrpr	%g0, PSTATE_ALT, %pstate

	/* Free some globals for our use. */
	sub	%g6, 24, %g6
	stx	%g1, [%g6]
	stx	%g2, [%g6 + 8]
	stx	%g3, [%g6 + 16]

	ldx	[PCPU(CURPCB)], %g1
	ldx	[%g1 + PCB_NSAVED], %g2

	sllx	%g2, 3, %g3
	add	%g3, %g1, %g3
	stx	%sp, [%g3 + PCB_RWSP]

	sllx	%g2, 7, %g3
	add	%g3, %g1, %g3
	SPILL(stx, %g3 + PCB_RW, 8, EMPTY)

	inc	%g2
	stx	%g2, [%g1 + PCB_NSAVED]

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl1_spill_topcb: pc=%lx sp=%#lx nsaved=%d"
	   , %g1, %g2, %g3, 7, 8, 9)
	rdpr	%tpc, %g2
	stx	%g2, [%g1 + KTR_PARM1]
	stx	%sp, [%g1 + KTR_PARM2]
	ldx	[PCPU(CURPCB)], %g2
	ldx	[%g2 + PCB_NSAVED], %g2
	stx	%g2, [%g1 + KTR_PARM3]
9:
#endif

	saved

	ldx	[%g6 + 16], %g3
	ldx	[%g6 + 8], %g2
	ldx	[%g6], %g1
	add	%g6, 24, %g6
	retry
END(tl1_spill_topcb)

	.macro	tl1_spill_bad	count
	.rept	\count
	tl1_wide T_SPILL
	.endr
	.endm

	.macro	tl1_fill_bad	count
	.rept	\count
	tl1_wide T_FILL
	.endr
	.endm

	.macro	tl1_breakpoint
	b,a	%xcc, tl1_breakpoint_trap
	.align	32
	.endm

ENTRY(tl1_breakpoint_trap)
	tl1_kstack
	sub	%sp, KF_SIZEOF, %sp
	flushw
	stx	%fp, [%sp + SPOFF + CCFSZ + KF_FP]
	mov	T_BREAKPOINT | T_KERNEL, %o0
	add	%sp, SPOFF + CCFSZ, %o1
	b	%xcc, tl1_trap
	 rdpr	%pil, %o2
END(tl1_breakpoint_trap)

	.macro	tl1_soft	count
	.rept	\count
	tl1_gen	T_SOFT | T_KERNEL
	.endr
	.endm

	.sect	.trap
	.align	0x8000
	.globl	tl0_base

tl0_base:
	tl0_reserved	1		! 0x0 unused
tl0_power_on:
	tl0_gen		T_POWER_ON	! 0x1 power on reset
tl0_watchdog:
	tl0_gen		T_WATCHDOG	! 0x2 watchdog rest
tl0_reset_ext:
	tl0_gen		T_RESET_EXT	! 0x3 externally initiated reset
tl0_reset_soft:
	tl0_gen		T_RESET_SOFT	! 0x4 software initiated reset
tl0_red_state:
	tl0_gen		T_RED_STATE	! 0x5 red state exception
	tl0_reserved	2		! 0x6-0x7 reserved
tl0_insn_excptn:
	tl0_gen		T_INSN_EXCPTN	! 0x8 instruction access exception
	tl0_reserved	1		! 0x9 reserved
tl0_insn_error:
	tl0_gen		T_INSN_ERROR	! 0xa instruction access error
	tl0_reserved	5		! 0xb-0xf reserved
tl0_insn_illegal:
	tl0_gen		T_INSN_ILLEGAL	! 0x10 illegal instruction
tl0_priv_opcode:
	tl0_gen		T_PRIV_OPCODE	! 0x11 privileged opcode
	tl0_reserved	14		! 0x12-0x1f reserved
tl0_fp_disabled:
	tl0_gen		T_FP_DISABLED	! 0x20 floating point disabled
tl0_fp_ieee:
	tl0_gen		T_FP_IEEE	! 0x21 floating point exception ieee
tl0_fp_other:
	tl0_gen		T_FP_OTHER	! 0x22 floating point exception other
tl0_tag_ovflw:
	tl0_gen		T_TAG_OVFLW	! 0x23 tag overflow
tl0_clean_window:
	clean_window			! 0x24 clean window
tl0_divide:
	tl0_gen		T_DIVIDE	! 0x28 division by zero
	tl0_reserved	7		! 0x29-0x2f reserved
tl0_data_excptn:
	tl0_data_excptn			! 0x30 data access exception
	tl0_reserved	1		! 0x31 reserved
tl0_data_error:
	tl0_gen		T_DATA_ERROR	! 0x32 data access error
	tl0_reserved	1		! 0x33 reserved
tl0_align:
	tl0_align			! 0x34 memory address not aligned
tl0_align_lddf:
	tl0_gen		T_ALIGN_LDDF	! 0x35 lddf memory address not aligned
tl0_align_stdf:
	tl0_gen		T_ALIGN_STDF	! 0x36 stdf memory address not aligned
tl0_priv_action:
	tl0_gen		T_PRIV_ACTION	! 0x37 privileged action
	tl0_reserved	9		! 0x38-0x40 reserved
tl0_intr_level:
	tl0_intr_level			! 0x41-0x4f interrupt level 1 to 15
	tl0_reserved	16		! 0x50-0x5f reserved
tl0_intr_vector:
	tl0_intr_vector			! 0x60 interrupt vector
tl0_watch_phys:
	tl0_gen		T_WATCH_PHYS	! 0x61 physical address watchpoint
tl0_watch_virt:
	tl0_gen		T_WATCH_VIRT	! 0x62 virtual address watchpoint
tl0_ecc:
	tl0_gen		T_ECC		! 0x63 corrected ecc error
tl0_immu_miss:
	tl0_immu_miss			! 0x64 fast instruction access mmu miss
tl0_dmmu_miss:
	tl0_dmmu_miss			! 0x68 fast data access mmu miss
tl0_dmmu_prot:
	tl0_dmmu_prot			! 0x6c fast data access protection
	tl0_reserved	16		! 0x70-0x7f reserved
tl0_spill_0_n:
	tl0_spill_0_n			! 0x80 spill 0 normal
tl0_spill_1_n:
	tl0_spill_1_n			! 0x84 spill 1 normal
tl0_spill_2_n:
	tl0_spill_2_n			! 0x88 spill 2 normal
tl0_spill_3_n:
	tl0_spill_3_n			! 0x8c spill 3 normal
	tl0_spill_bad	12		! 0x90-0xbf spill normal, other
tl0_fill_0_n:
	tl0_fill_0_n			! 0xc0 fill 0 normal
tl0_fill_1_n:
	tl0_fill_1_n			! 0xc4 fill 1 normal
tl0_fill_2_n:
	tl0_fill_2_n			! 0xc8 fill 2 normal
tl0_fill_3_n:
	tl0_fill_3_n			! 0xcc fill 3 normal
	tl0_fill_bad	12		! 0xc4-0xff fill normal, other
tl0_sun_syscall:
	tl0_reserved	1		! 0x100 sun system call
tl0_breakpoint:
	tl0_gen		T_BREAKPOINT	! 0x101 breakpoint
	tl0_soft	6		! 0x102-0x107 trap instruction
	tl0_soft	1		! 0x108 SVr4 syscall
	tl0_gen		T_SYSCALL	! 0x109 BSD syscall
	tl0_soft	118		! 0x110-0x17f trap instruction
	tl0_reserved	128		! 0x180-0x1ff reserved

tl1_base:
	tl1_reserved	1		! 0x200 unused
tl1_power_on:
	tl1_gen		T_POWER_ON	! 0x201 power on reset
tl1_watchdog:
	tl1_gen		T_WATCHDOG	! 0x202 watchdog rest
tl1_reset_ext:
	tl1_gen		T_RESET_EXT	! 0x203 externally initiated reset
tl1_reset_soft:
	tl1_gen		T_RESET_SOFT	! 0x204 software initiated reset
tl1_red_state:
	tl1_gen		T_RED_STATE	! 0x205 red state exception
	tl1_reserved	2		! 0x206-0x207 reserved
tl1_insn_excptn:
	tl1_insn_excptn			! 0x208 instruction access exception
	tl1_reserved	1		! 0x209 reserved
tl1_insn_error:
	tl1_gen		T_INSN_ERROR	! 0x20a instruction access error
	tl1_reserved	5		! 0x20b-0x20f reserved
tl1_insn_illegal:
	tl1_gen		T_INSN_ILLEGAL	! 0x210 illegal instruction
tl1_priv_opcode:
	tl1_gen		T_PRIV_OPCODE	! 0x211 privileged opcode
	tl1_reserved	14		! 0x212-0x21f reserved
tl1_fp_disabled:
	tl1_gen		T_FP_DISABLED	! 0x220 floating point disabled
tl1_fp_ieee:
	tl1_gen		T_FP_IEEE	! 0x221 floating point exception ieee
tl1_fp_other:
	tl1_gen		T_FP_OTHER	! 0x222 floating point exception other
tl1_tag_ovflw:
	tl1_gen		T_TAG_OVFLW	! 0x223 tag overflow
tl1_clean_window:
	clean_window			! 0x224 clean window
tl1_divide:
	tl1_gen		T_DIVIDE	! 0x228 division by zero
	tl1_reserved	7		! 0x229-0x22f reserved
tl1_data_excptn:
	tl1_data_excptn			! 0x230 data access exception
	tl1_reserved	1		! 0x231 reserved
tl1_data_error:
	tl1_gen		T_DATA_ERROR	! 0x232 data access error
	tl1_reserved	1		! 0x233 reserved
tl1_align:
	tl1_align			! 0x234 memory address not aligned
tl1_align_lddf:
	tl1_gen		T_ALIGN_LDDF	! 0x235 lddf memory address not aligned
tl1_align_stdf:
	tl1_gen		T_ALIGN_STDF	! 0x236 stdf memory address not aligned
tl1_priv_action:
	tl1_gen		T_PRIV_ACTION	! 0x237 privileged action
	tl1_reserved	9		! 0x238-0x240 reserved
tl1_intr_level:
	tl1_intr_level			! 0x241-0x24f interrupt level 1 to 15
	tl1_reserved	16		! 0x250-0x25f reserved
tl1_intr_vector:
	tl1_intr_vector			! 0x260 interrupt vector
tl1_watch_phys:
	tl1_gen		T_WATCH_PHYS	! 0x261 physical address watchpoint
tl1_watch_virt:
	tl1_gen		T_WATCH_VIRT	! 0x262 virtual address watchpoint
tl1_ecc:
	tl1_gen		T_ECC		! 0x263 corrected ecc error
tl1_immu_miss:
	tl1_immu_miss			! 0x264 fast instruction access mmu miss
tl1_dmmu_miss:
	tl1_dmmu_miss			! 0x268 fast data access mmu miss
tl1_dmmu_prot:
	tl1_dmmu_prot			! 0x26c fast data access protection
	tl1_reserved	16		! 0x270-0x27f reserved
tl1_spill_0_n:
	tl1_spill_0_n			! 0x280 spill 0 normal
	tl1_spill_bad	3		! 0x284-0x28f spill normal
tl1_spill_4_n:
	tl1_spill_4_n			! 0x290 spill 4 normal
tl1_spill_5_n:
	tl1_spill_5_n			! 0x294 spill 5 normal
tl1_spill_6_n:
	tl1_spill_6_n			! 0x298 spill 6 normal
tl1_spill_7_n:
	tl1_spill_7_n			! 0x29c spill 7 normal
tl1_spill_0_o:
	tl1_spill_0_o			! 0x2a0 spill 0 other
tl1_spill_1_o:
	tl1_spill_1_o			! 0x2a4 spill 1 other
tl1_spill_2_o:
	tl1_spill_2_o			! 0x2a8 spill 2 other
tl1_spill_3_o:
	tl1_spill_3_o			! 0x2ac spill 3 other
	tl1_spill_bad	4		! 0x2a0-0x2bf spill other
tl1_fill_0_n:
	tl1_fill_0_n			! 0x2c0 fill 0 normal
	tl1_fill_bad	3		! 0x2c4-0x2cf fill normal
tl1_fill_4_n:
	tl1_fill_4_n			! 0x2d0 fill 4 normal
tl1_fill_5_n:
	tl1_fill_5_n			! 0x2d4 fill 5 normal
tl1_fill_6_n:
	tl1_fill_6_n			! 0x2d8 fill 6 normal
tl1_fill_7_n:
	tl1_fill_7_n			! 0x2dc fill 7 normal
	tl1_fill_bad	8		! 0x2e0-0x2ff fill other
	tl1_reserved	1		! 0x300 trap instruction
tl1_breakpoint:
	tl1_breakpoint			! 0x301 breakpoint
	tl1_gen		T_RESTOREWP	! 0x302 restore watchpoint (debug)
	tl1_soft	125		! 0x303-0x37f trap instruction
	tl1_reserved	128		! 0x380-0x3ff reserved

/*
 * User trap entry point.
 *
 * void tl0_trap(u_long type, u_long arg, u_long pil, u_long wstate)
 *
 * The following setup has been performed:
 *	- the windows have been split and the active user window has been saved
 *	  (maybe just to the pcb)
 *	- we are on the current kernel stack and a frame has been setup, there
 *	  may be extra trap specific stuff below the frame
 *	- we are on alternate globals and interrupts are disabled
 *
 * We build a trapframe, switch to normal globals, enable interrupts and call
 * trap.
 *
 * NOTE: Due to a chip bug, we must save the trap state registers in memory
 * early.
 *
 * NOTE: We must be very careful setting up the per-cpu pointer.  We know that
 * it has been pre-set in alternate globals, so we read it from there and setup
 * the normal %g7 *before* enabling interrupts.  This avoids any possibility
 * of cpu migration and using the wrong globalp.
 */
ENTRY(tl0_trap)
	/*
	 * Force kernel store order.
	 */
	wrpr	%g0, PSTATE_ALT, %pstate

	sub	%sp, TF_SIZEOF, %sp
	
	rdpr	%tstate, %l0
	stx	%l0, [%sp + SPOFF + CCFSZ + TF_TSTATE]
	rdpr	%tpc, %l1
	stx	%l1, [%sp + SPOFF + CCFSZ + TF_TPC]
	rdpr	%tnpc, %l2
	stx	%l2, [%sp + SPOFF + CCFSZ + TF_TNPC]

	stx	%o0, [%sp + SPOFF + CCFSZ + TF_TYPE]
	stx	%o1, [%sp + SPOFF + CCFSZ + TF_ARG]
	stx	%o2, [%sp + SPOFF + CCFSZ + TF_PIL]
	stx	%o3, [%sp + SPOFF + CCFSZ + TF_WSTATE]

.Ltl0_trap_fill:
	mov	%g7, %l0
	wrpr	%g0, PSTATE_NORMAL, %pstate
	mov	%l0, %g7	/* set up the normal %g7 */
	wrpr	%g0, PSTATE_KERNEL, %pstate

	stx	%g1, [%sp + SPOFF + CCFSZ + TF_G1]
	stx	%g2, [%sp + SPOFF + CCFSZ + TF_G2]
	stx	%g3, [%sp + SPOFF + CCFSZ + TF_G3]
	stx	%g4, [%sp + SPOFF + CCFSZ + TF_G4]
	stx	%g5, [%sp + SPOFF + CCFSZ + TF_G5]
	stx	%g6, [%sp + SPOFF + CCFSZ + TF_G6]
	stx	%g7, [%sp + SPOFF + CCFSZ + TF_G7]

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl0_trap: p=%p type=%#x arg=%#lx pil=%#lx ws=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	ldx	[PCPU(CURPROC)], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	stx	%o0, [%g1 + KTR_PARM2]
	stx	%o1, [%g1 + KTR_PARM3]
	stx	%o2, [%g1 + KTR_PARM4]
	stx	%o3, [%g1 + KTR_PARM5]
9:
#endif

	stx	%i0, [%sp + SPOFF + CCFSZ + TF_O0]
	stx	%i1, [%sp + SPOFF + CCFSZ + TF_O1]
	stx	%i2, [%sp + SPOFF + CCFSZ + TF_O2]
	stx	%i3, [%sp + SPOFF + CCFSZ + TF_O3]
	stx	%i4, [%sp + SPOFF + CCFSZ + TF_O4]
	stx	%i5, [%sp + SPOFF + CCFSZ + TF_O5]
	stx	%i6, [%sp + SPOFF + CCFSZ + TF_O6]
	stx	%i7, [%sp + SPOFF + CCFSZ + TF_O7]

.Ltl0_trap_spill:
	call	trap
	 add	%sp, CCFSZ + SPOFF, %o0
	
	/* Fallthough. */
END(tl0_trap)

/* Return to tl0 (user process). */
ENTRY(tl0_ret)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl0_ret: p=%p (%s) pil=%#lx sflag=%#x"
	    , %g1, %g2, %g3, 7, 8, 9)
	ldx	[PCPU(CURPROC)], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	add	%g2, P_COMM, %g3
	stx	%g3, [%g1 + KTR_PARM2]
	rdpr	%pil, %g3
	stx	%g3, [%g1 + KTR_PARM3]
	lduw	[%g2 + P_SFLAG], %g3
	stx	%g3, [%g1 + KTR_PARM4]
9:
#endif

	wrpr	%g0, PIL_TICK, %pil
	ldx	[PCPU(CURPROC)], %o0
	lduw	[%o0 + P_SFLAG], %o1
	and	%o1, PS_ASTPENDING | PS_NEEDRESCHED, %o1
	brz,pt	%o1, 1f
	 nop
	call	ast
	 add	%sp, CCFSZ + SPOFF, %o0

1:	ldx	[PCPU(CURPCB)], %o0
	ldx	[%o0 + PCB_NSAVED], %o1
	mov	T_SPILL, %o0
	brnz,a,pn %o1, .Ltl0_trap_spill
	 stx	%o0, [%sp + SPOFF + CCFSZ + TF_TYPE]

	ldx	[%sp + SPOFF + CCFSZ + TF_G1], %g1
	ldx	[%sp + SPOFF + CCFSZ + TF_G2], %g2
	ldx	[%sp + SPOFF + CCFSZ + TF_G3], %g3
	ldx	[%sp + SPOFF + CCFSZ + TF_G4], %g4
	ldx	[%sp + SPOFF + CCFSZ + TF_G5], %g5
	ldx	[%sp + SPOFF + CCFSZ + TF_G6], %g6
	ldx	[%sp + SPOFF + CCFSZ + TF_G7], %g7

	ldx	[%sp + SPOFF + CCFSZ + TF_O0], %i0
	ldx	[%sp + SPOFF + CCFSZ + TF_O1], %i1
	ldx	[%sp + SPOFF + CCFSZ + TF_O2], %i2
	ldx	[%sp + SPOFF + CCFSZ + TF_O3], %i3
	ldx	[%sp + SPOFF + CCFSZ + TF_O4], %i4
	ldx	[%sp + SPOFF + CCFSZ + TF_O5], %i5
	ldx	[%sp + SPOFF + CCFSZ + TF_O6], %i6
	ldx	[%sp + SPOFF + CCFSZ + TF_O7], %i7

	ldx	[%sp + SPOFF + CCFSZ + TF_PIL], %l0
	ldx	[%sp + SPOFF + CCFSZ + TF_TSTATE], %l1
	ldx	[%sp + SPOFF + CCFSZ + TF_TPC], %l2
	ldx	[%sp + SPOFF + CCFSZ + TF_TNPC], %l3
	ldx	[%sp + SPOFF + CCFSZ + TF_WSTATE], %l4

	wrpr	%g0, PSTATE_ALT, %pstate

	wrpr	%l0, 0, %pil

	wrpr	%l1, 0, %tstate
	wrpr	%l2, 0, %tpc
	wrpr	%l3, 0, %tnpc

	/*
	 * Restore the user window state.
	 * NOTE: whenever we come here, it should be with %canrestore = 0.
	 */
	srlx	%l4, WSTATE_USERSHIFT, %g1
	wrpr	%g1, WSTATE_TRANSITION, %wstate
	rdpr	%otherwin, %g2
	wrpr	%g2, 0, %canrestore
	wrpr	%g0, 0, %otherwin
	wrpr	%g2, 0, %cleanwin

	/*
	 * If this instruction causes a fill trap which fails to fill a window
	 * from the user stack, we will resume at tl0_ret_fill_end and call
	 * back into the kernel.
	 */
	restore
tl0_ret_fill:

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl0_ret: return p=%#lx pil=%#lx ts=%#lx pc=%#lx sp=%#lx"
	    , %g2, %g3, %g4, 7, 8, 9)
	ldx	[PCPU(CURPROC)], %g3
	stx	%g3, [%g2 + KTR_PARM1]
	rdpr	%tstate, %g3
	stx	%g3, [%g2 + KTR_PARM2]
	rdpr	%tpc, %g3
	stx	%g3, [%g2 + KTR_PARM3]
	stx	%sp, [%g2 + KTR_PARM4]
	stx	%g1, [%g2 + KTR_PARM5]
9:
#endif

	wrpr	%g1, 0, %wstate
	retry
tl0_ret_fill_end:

#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "tl0_ret: fill magic wstate=%#lx sp=%#lx"
	    , %l0, %l1, %l2, 7, 8, 9)
	stx	%l4, [%l0 + KTR_PARM1]
	stx	%sp, [%l0 + KTR_PARM2]
9:
#endif

	/*
	 * The fill failed and magic has been preformed.  Call trap again,
	 * which will copyin the window on the user's behalf.
	 */
	wrpr	%l4, 0, %wstate
	mov	T_FILL, %o0
	b	%xcc, .Ltl0_trap_fill
	 stx	%o0, [%sp + SPOFF + CCFSZ + TF_TYPE]
END(tl0_ret)

/*
 * Kernel trap entry point
 *
 * void tl1_trap(u_long type, u_long arg, u_long pil)
 *
 * This is easy because the stack is already setup and the windows don't need
 * to be split.  We build a trapframe and call trap(), the same as above, but
 * the outs don't need to be saved.
 *
 * NOTE: See comments above tl0_trap for song and dance about chip bugs and
 * setting up globalp.
 */
ENTRY(tl1_trap)
	sub	%sp, TF_SIZEOF, %sp

	rdpr	%tstate, %l0
	stx	%l0, [%sp + SPOFF + CCFSZ + TF_TSTATE]
	rdpr	%tpc, %l1
	stx	%l1, [%sp + SPOFF + CCFSZ + TF_TPC]
	rdpr	%tnpc, %l2
	stx	%l2, [%sp + SPOFF + CCFSZ + TF_TNPC]

#if KTR_COMPILE & KTR_CT1
	setx	trap_mask, %l4, %l3
	andn	%o1, T_KERNEL, %l4
	mov	1, %l5
	sllx	%l5, %l4, %l4
	ldx	[%l3], %l5
	and	%l4, %l5, %l4
	brz	%l4, 9f
	 nop
	CATR(KTR_CT1, "tl1_trap: p=%p pil=%#lx type=%#lx arg=%#lx pc=%#lx"
	    , %l3, %l4, %l5, 7, 8, 9)
	ldx	[PCPU(CURPROC)], %l4
	stx	%l4, [%l3 + KTR_PARM1]
#if 0
	add	%l4, P_COMM, %l4
	stx	%l4, [%l3 + KTR_PARM2]
#else
	stx	%o2, [%l3 + KTR_PARM2]
#endif
	andn	%o0, T_KERNEL, %l4
	stx	%l4, [%l3 + KTR_PARM3]
	stx	%o1, [%l3 + KTR_PARM4]
	stx	%l1, [%l3 + KTR_PARM5]
9:
#endif

	wrpr	%g0, 1, %tl
	/* We may have trapped before %g7 was set up correctly. */
	mov	%g7, %l0
	wrpr	%g0, PSTATE_NORMAL, %pstate
	mov	%l0, %g7
	wrpr	%g0, PSTATE_KERNEL, %pstate

	stx	%o0, [%sp + SPOFF + CCFSZ + TF_TYPE]
	stx	%o1, [%sp + SPOFF + CCFSZ + TF_ARG]
	stx	%o2, [%sp + SPOFF + CCFSZ + TF_PIL]

	stx	%g1, [%sp + SPOFF + CCFSZ + TF_G1]
	stx	%g2, [%sp + SPOFF + CCFSZ + TF_G2]
	stx	%g3, [%sp + SPOFF + CCFSZ + TF_G3]
	stx	%g4, [%sp + SPOFF + CCFSZ + TF_G4]
	stx	%g5, [%sp + SPOFF + CCFSZ + TF_G5]
	stx	%g6, [%sp + SPOFF + CCFSZ + TF_G6]

	call	trap
	 add	%sp, CCFSZ + SPOFF, %o0

	ldx	[%sp + SPOFF + CCFSZ + TF_G1], %g1
	ldx	[%sp + SPOFF + CCFSZ + TF_G2], %g2
	ldx	[%sp + SPOFF + CCFSZ + TF_G3], %g3
	ldx	[%sp + SPOFF + CCFSZ + TF_G4], %g4
	ldx	[%sp + SPOFF + CCFSZ + TF_G5], %g5
	ldx	[%sp + SPOFF + CCFSZ + TF_G6], %g6

	ldx	[%sp + SPOFF + CCFSZ + TF_PIL], %l0
	ldx	[%sp + SPOFF + CCFSZ + TF_TSTATE], %l1
	ldx	[%sp + SPOFF + CCFSZ + TF_TPC], %l2
	ldx	[%sp + SPOFF + CCFSZ + TF_TNPC], %l3

	wrpr	%g0, PSTATE_ALT, %pstate

	wrpr	%l0, 0, %pil

	wrpr	%g0, 2, %tl
	wrpr	%l1, 0, %tstate
	wrpr	%l2, 0, %tpc
	wrpr	%l3, 0, %tnpc

#if KTR_COMPILE & KTR_CT1
	ldx	[%sp + SPOFF + CCFSZ + TF_TYPE], %l5
	andn	%l5, T_KERNEL, %l4
	mov	1, %l5
	sllx	%l5, %l4, %l4
	setx	trap_mask, %l4, %l3
	ldx	[%l3], %l5
	and	%l4, %l5, %l4
	brz	%l4, 9f
	 nop
	CATR(KTR_CT1, "tl1_trap: return p=%p pil=%#lx sp=%#lx pc=%#lx"
	    , %l3, %l4, %l5, 7, 8, 9)
	ldx	[PCPU(CURPROC)], %l4
	stx	%l4, [%l3 + KTR_PARM1]
	stx	%l0, [%l3 + KTR_PARM2]
	stx	%sp, [%l3 + KTR_PARM3]
	stx	%l2, [%l3 + KTR_PARM4]
9:
#endif

	restore
	retry
END(tl1_trap)

/*
 * Freshly forked processes come here when switched to for the first time.
 * The arguments to fork_exit() have been setup in the locals, we must move
 * them to the outs.
 */
ENTRY(fork_trampoline)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "fork_trampoline: p=%p (%s) cwp=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	ldx	[PCPU(CURPROC)], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	add	%g2, P_COMM, %g2
	stx	%g2, [%g1 + KTR_PARM2]
	rdpr	%cwp, %g2
	stx	%g2, [%g1 + KTR_PARM3]
9:
#endif
	mov	%l0, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	fork_exit
	 nop
	b,a	%xcc, tl0_ret
END(fork_trampoline)
