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
 * $FreeBSD$
 */

#include "opt_ddb.h"

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/trap.h>

#include "assym.s"

#define	SPILL(storer, base, asi) \
	storer	%l0, [base + F_L0] asi ; \
	storer	%l1, [base + F_L1] asi ; \
	storer	%l2, [base + F_L2] asi ; \
	storer	%l3, [base + F_L3] asi ; \
	storer	%l4, [base + F_L4] asi ; \
	storer	%l5, [base + F_L5] asi ; \
	storer	%l6, [base + F_L6] asi ; \
	storer	%l7, [base + F_L7] asi ; \
	storer	%i0, [base + F_I0] asi ; \
	storer	%i1, [base + F_I1] asi ; \
	storer	%i2, [base + F_I2] asi ; \
	storer	%i3, [base + F_I3] asi ; \
	storer	%i4, [base + F_I4] asi ; \
	storer	%i5, [base + F_I5] asi ; \
	storer	%i6, [base + F_I6] asi ; \
	storer	%i7, [base + F_I7] asi

#define	FILL(loader, base, asi) \
	loader	[base + F_L0] asi, %l0 ; \
	loader	[base + F_L1] asi, %l1 ; \
	loader	[base + F_L2] asi, %l2 ; \
	loader	[base + F_L3] asi, %l3 ; \
	loader	[base + F_L4] asi, %l4 ; \
	loader	[base + F_L5] asi, %l5 ; \
	loader	[base + F_L6] asi, %l6 ; \
	loader	[base + F_L7] asi, %l7 ; \
	loader	[base + F_I0] asi, %i0 ; \
	loader	[base + F_I1] asi, %i1 ; \
	loader	[base + F_I2] asi, %i2 ; \
	loader	[base + F_I3] asi, %i3 ; \
	loader	[base + F_I4] asi, %i4 ; \
	loader	[base + F_I5] asi, %i5 ; \
	loader	[base + F_I6] asi, %i6 ; \
	loader	[base + F_I7] asi, %i7

DATA(intrnames)
	.asciz	"foo"
DATA(eintrnames)

DATA(intrcnt)
	.long	0
DATA(eintrcnt)

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

	.macro	tl0_gen		type
	save	%sp, -CCFSZ, %sp
	b	%xcc, tl1_trap
	 mov	\type, %o0
	.align	32
	.endm

	.macro	tl0_wide	type
	save	%sp, -CCFSZ, %sp
	b	%xcc, tl1_trap
	 mov	\type, %o0
	.align	128
	.endm

	.macro	tl0_reserved	count
	.rept	\count
	tl0_gen	T_RESERVED
	.endr
	.endm

	.macro	tl0_intr_level
	tl0_reserved 15
	.endm

	.macro	tl0_intr_vector
	tl0_gen	0
	.endm

	.macro	tl0_immu_miss
	tl0_wide T_IMMU_MISS
	.endm

	.macro	tl0_dmmu_miss
	tl0_wide T_DMMU_MISS
	.endm

	.macro	tl0_dmmu_prot
	tl0_wide T_DMMU_PROT
	.endm

	.macro	tl0_spill_0_n
	wr	%g0, ASI_AIUP, %asi
	SPILL(stxa, %sp + SPOFF, %asi)
	saved
	retry
	.align	128
	.endm

	.macro	tl0_spill_bad	count
	.rept	\count
	tl0_wide T_SPILL
	.endr
	.endm

	.macro	tl0_fill_0_n
	wr	%g0, ASI_AIUP, %asi
	FILL(ldxa, %sp + SPOFF, %asi)
	restored
	retry
	.align	128
	.endm

	.macro	tl0_fill_bad	count
	.rept	\count
	tl0_wide T_FILL
	.endr
	.endm

	.macro	tl0_soft	count
	tl0_reserved \count
	.endm

	.macro	tl1_gen		type
	save	%sp, -CCFSZ, %sp
	b	%xcc, tl1_trap
	 mov	\type | T_KERNEL, %o0
	.align	32
	.endm

	.macro	tl1_wide	type
	save	%sp, -CCFSZ, %sp
	b	%xcc, tl1_trap
	 mov	\type | T_KERNEL, %o0
	.align	128
	.endm

	.macro	tl1_reserved	count
	.rept	\count
	tl1_gen	T_RESERVED
	.endr
	.endm

	.macro	tl1_insn_excptn
	rdpr	%pstate, %g1
	wrpr	%g1, PSTATE_MG | PSTATE_AG, %pstate
	save	%sp, -CCFSZ, %sp
	b	%xcc, tl1_trap
	 mov	T_INSN_EXCPTN | T_KERNEL, %o0
	.align	32
	.endm

	.macro	tl1_align
	b	%xcc, tl1_sfsr_trap
	 nop
	.align	32
	.endm

ENTRY(tl1_sfsr_trap)
	wr	%g0, ASI_DMMU, %asi
	ldxa	[%g0 + AA_DMMU_SFAR] %asi, %g1
	ldxa	[%g0 + AA_DMMU_SFSR] %asi, %g2
	stxa	%g0, [%g0 + AA_DMMU_SFSR] %asi
	membar	#Sync
	save	%sp, -(CCFSZ + MF_SIZEOF), %sp
	stx	%g1, [%sp + SPOFF + CCFSZ + MF_SFAR]
	stx	%g2, [%sp + SPOFF + CCFSZ + MF_SFSR]
	mov	T_ALIGN | T_KERNEL, %o0
	b	%xcc, tl1_trap
	 add	%sp, SPOFF + CCFSZ, %o1
END(tl1_sfsr_trap)

	.macro	tl1_intr_level
	tl1_reserved 15
	.endm

	.macro	tl1_intr_vector
	rdpr	%pstate, %g1
	wrpr	%g1, PSTATE_IG | PSTATE_AG, %pstate
	save	%sp, -CCFSZ, %sp
	b	%xcc, tl1_trap
	 mov	T_INTERRUPT | T_KERNEL, %o0
	.align	8
	.endm

	.macro	tl1_immu_miss
	rdpr	%pstate, %g1
	wrpr	%g1, PSTATE_MG | PSTATE_AG, %pstate
	save	%sp, -CCFSZ, %sp
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
	brnz,pn	%g2, 2f
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
	 * If the mod bit is clear, clear the write bit too.
	 */
1:	andcc	%g5, TD_MOD, %g1
	movz	%xcc, TD_W, %g1
	andn	%g5, %g1, %g5

	/*
	 * Load the tte data into the TLB and retry the instruction.
	 */
	stxa	%g5, [%g0] ASI_DTLB_DATA_IN_REG
	retry

	/*
	 * For now just bail.  This might cause a red state exception,
	 * but oh well.
	 */
2:	DEBUGGER()
	.align	128
	.endm

	.macro	tl1_dmmu_prot
	rdpr	%pstate, %g1
	wrpr	%g1, PSTATE_MG | PSTATE_AG, %pstate
	save	%sp, -CCFSZ, %sp
	b	%xcc, tl1_trap
	 mov	T_DMMU_PROT | T_KERNEL, %o0
	.align	128
	.endm

	.macro	tl1_spill_0_n
	SPILL(stx, %sp + SPOFF, EMPTY)
	saved
	retry
	.align	128
	.endm

	.macro	tl1_spill_bad	count
	.rept	\count
	tl1_wide T_SPILL
	.endr
	.endm

	.macro	tl1_fill_0_n
	FILL(ldx, %sp + SPOFF, EMPTY)
	restored
	retry
	.align	128
	.endm

	.macro	tl1_fill_bad	count
	.rept	\count
	tl1_wide T_FILL
	.endr
	.endm

	.macro	tl1_breakpoint
	b	%xcc, tl1_breakpoint_trap
	 nop
	.align	32
	.endm

ENTRY(tl1_breakpoint_trap)
	save	%sp, -(CCFSZ + KF_SIZEOF), %sp
	flushw
	stx	%fp, [%sp + SPOFF + CCFSZ + KF_FP]
	mov	T_BREAKPOINT | T_KERNEL, %o0
	b	%xcc, tl1_trap
	 add	%sp, SPOFF + CCFSZ, %o1
END(tl1_breakpoint_trap)

	.macro	tl1_soft	count
	tl1_reserved \count
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
	tl0_gen		T_DATA_EXCPTN	! 0x30 data access exception
	tl0_reserved	1		! 0x31 reserved
tl0_data_error:
	tl0_gen		T_DATA_ERROR	! 0x32 data access error
	tl0_reserved	1		! 0x33 reserved
tl0_align:
	tl0_gen		T_ALIGN		! 0x34 memory address not aligned
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
tl0_spill_bad:
	tl0_spill_bad	15		! 0x84-0xbf spill normal, other
tl0_fill_0_n:
	tl0_fill_0_n			! 0xc0 fill 0 normal
tl0_fill_bad:
	tl0_fill_bad	15		! 0xc4-0xff fill normal, other
tl0_sun_syscall:
	tl0_reserved	1		! 0x100 sun system call
tl0_breakpoint:
	tl0_gen		T_BREAKPOINT	! 0x101 breakpoint
	tl0_soft	126		! 0x102-0x17f trap instruction
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
	tl1_gen		T_DATA_EXCPTN	! 0x230 data access exception
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
tl1_spill_bad:
	tl1_spill_bad	15		! 0x284-0x2bf spill normal, other
tl1_fill_0_n:
	tl1_fill_0_n			! 0x2c0 fill 0 normal
tl1_fill_bad:
	tl1_fill_bad	15		! 0x2c4-0x2ff fill normal, other
	tl1_reserved	1		! 0x300 trap instruction
tl1_breakpoint:
	tl1_breakpoint			! 0x301 breakpoint
	tl1_soft	126		! 0x302-0x37f trap instruction
	tl1_reserved	128		! 0x380-0x3ff reserved

ENTRY(tl0_trap)
	/* In every trap from tl0, we need to set PSTATE.PEF. */
	illtrap
END(tl0_trap)

/*
 * void tl1_trap(u_long o0, u_long o1, u_long o2, u_long type)
 */
ENTRY(tl1_trap)
	sub	%sp, TF_SIZEOF, %sp
	rdpr	%tstate, %l0
	stx	%l0, [%sp + SPOFF + CCFSZ + TF_TSTATE]
	rdpr	%tpc, %l1
	stx	%l1, [%sp + SPOFF + CCFSZ + TF_TPC]
	rdpr	%tnpc, %l2
	stx	%l2, [%sp + SPOFF + CCFSZ + TF_TNPC]

	wrpr	%g0, 1, %tl
	rdpr	%pstate, %l7
	wrpr	%l7, PSTATE_AG | PSTATE_IE, %pstate

	stx	%o0, [%sp + SPOFF + CCFSZ + TF_TYPE]
	stx	%o1, [%sp + SPOFF + CCFSZ + TF_ARG]

	stx	%g1, [%sp + SPOFF + CCFSZ + TF_G1]
	stx	%g2, [%sp + SPOFF + CCFSZ + TF_G2]
	stx	%g3, [%sp + SPOFF + CCFSZ + TF_G3]
	stx	%g4, [%sp + SPOFF + CCFSZ + TF_G4]
	stx	%g5, [%sp + SPOFF + CCFSZ + TF_G5]
	stx	%g6, [%sp + SPOFF + CCFSZ + TF_G6]
	stx	%g7, [%sp + SPOFF + CCFSZ + TF_G7]

	call	trap
	 add	%sp, CCFSZ + SPOFF, %o0

	ldx	[%sp + SPOFF + CCFSZ + TF_G1], %g1
	ldx	[%sp + SPOFF + CCFSZ + TF_G2], %g2
	ldx	[%sp + SPOFF + CCFSZ + TF_G3], %g3
	ldx	[%sp + SPOFF + CCFSZ + TF_G4], %g4
	ldx	[%sp + SPOFF + CCFSZ + TF_G5], %g5
	ldx	[%sp + SPOFF + CCFSZ + TF_G6], %g6
	ldx	[%sp + SPOFF + CCFSZ + TF_G7], %g7

	ldx	[%sp + SPOFF + CCFSZ + TF_TSTATE], %l0
	ldx	[%sp + SPOFF + CCFSZ + TF_TPC], %l1
	ldx	[%sp + SPOFF + CCFSZ + TF_TNPC], %l2

	rdpr	%pstate, %o0
	wrpr	%o0, PSTATE_AG | PSTATE_IE, %pstate

	wrpr	%g0, 2, %tl
	wrpr	%l0, 0, %tstate
	wrpr	%l1, 0, %tpc
	wrpr	%l2, 0, %tnpc

	restore
	retry
END(tl1_trap)

ENTRY(fork_trampoline)
	mov	%l0, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	call	fork_exit
	 nop
	DEBUGGER()
END(fork_trampoline)
