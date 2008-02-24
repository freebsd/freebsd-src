/*-
 * Copyright (c) 2001 Jake Burkholder.
 * Copyright (c) 2006 Kip Macy
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
 * $FreeBSD: src/sys/sun4v/include/asmacros.h,v 1.2 2006/11/15 03:16:30 kmacy Exp $
 */

#ifndef	_MACHINE_ASMACROS_H_
#define	_MACHINE_ASMACROS_H_

#ifdef _KERNEL

/*
 *  %g7 points to per-cpu data.
 */
#define	PCPU_REG	%g7


#ifdef LOCORE

/*
 * Atomically decrement an integer in memory.
 */
#define	ATOMIC_DEC_INT(r1, r2, r3) \
	lduw	[r1], r2 ; \
9:	sub	r2, 1, r3 ; \
	casa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%icc, 9b ; \
	 mov	r3, r2

/*
 * Atomically increment an integer in memory.
 */
#define	ATOMIC_INC_INT(r1, r2, r3) \
	lduw	[r1], r2 ; \
9:	add	r2, 1, r3 ; \
	casa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%icc, 9b ; \
	 mov	r3, r2

/*
 * Atomically increment an u_long in memory.
 */
#define	ATOMIC_INC_ULONG(r1, r2, r3) \
	ldx	[r1], r2 ; \
9:	add	r2, 1, r3 ; \
	casxa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%icc, 9b ; \
	 mov	r3, r2

/*
 * Atomically clear a number of bits of an integer in memory.
 */
#define	ATOMIC_CLEAR_INT(r1, r2, r3, bits) \
	lduw	[r1], r2 ; \
9:	andn	r2, bits, r3 ; \
	casa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%icc, 9b ; \
	 mov	r3, r2

#define	PCPU(member)	PCPU_REG + PC_ ## member
#define	PCPU_ADDR(member, reg) \
	add	PCPU_REG, PC_ ## member, reg

#define	DEBUGGER() \
	ta	%xcc, 1

#define	PANIC(msg, r1) \
	.sect	.rodata ; \
9:	.asciz	msg ; \
	.previous ; \
	SET(9b, r1, %o0) ; \
	call	panic ; \
	 nop

#ifdef INVARIANTS
#define	KASSERT(r1, msg) \
	brnz	r1, 8f ; \
	 nop ; \
	PANIC(msg, r1) ; \
8:
#else
#define	KASSERT(r1, msg)
#endif

#define	PUTS(msg, r1) \
	.sect	.rodata ; \
9:	.asciz	msg ; \
	.previous ; \
	SET(9b, r1, %o0) ; \
	call	printf ; \
	 nop

#define	_ALIGN_DATA	.align 8

#define	DATA(name) \
	.data ; \
	_ALIGN_DATA ; \
	.globl	name ; \
	.type	name, @object ; \
name:

#define	EMPTY

#define GET_MMFSA_SCRATCH(reg)             \
	ldxa [%g0 + %g0]ASI_SCRATCHPAD, reg;


#define GET_PCPU_PHYS_SCRATCH(tmp)                      \
        sethi %uhi(VM_MIN_DIRECT_ADDRESS), tmp;         \
        mov  SCRATCH_REG_PCPU, PCPU_REG;                \
        sllx tmp, 32, tmp;                              \
        ldxa [%g0 + PCPU_REG]ASI_SCRATCHPAD, PCPU_REG;  \
        andn PCPU_REG, tmp, PCPU_REG

#define GET_PCPU_SCRATCH                    \
	mov  SCRATCH_REG_PCPU, PCPU_REG;    \
        ldxa [%g0 + PCPU_REG]ASI_SCRATCHPAD, PCPU_REG;

#define GET_PCPU_SCRATCH_SLOW(reg)          \
	mov  SCRATCH_REG_PCPU, reg;    \
        ldxa [reg]ASI_SCRATCHPAD, PCPU_REG;

#define GET_HASH_SCRATCH_USER(reg)	   \
        mov SCRATCH_REG_HASH_USER, reg;    \
	ldxa [%g0 + reg]ASI_SCRATCHPAD, reg;

#define GET_HASH_SCRATCH_KERNEL(reg)   \
        mov SCRATCH_REG_HASH_KERNEL, reg;  \
	ldxa [%g0 + reg]ASI_SCRATCHPAD, reg; 

#define GET_HASH_PHYS_SCRATCH_USER(tmp, reg)	\
	sethi %uhi(VM_MIN_DIRECT_ADDRESS), tmp; \
        mov SCRATCH_REG_HASH_USER, reg;         \
	sllx tmp, 32, tmp;                      \
	ldxa [%g0 + reg]ASI_SCRATCHPAD, reg;    \
	andn reg, tmp, reg;

#define GET_HASH_PHYS_SCRATCH_KERNEL(tmp, reg)    \
	sethi %uhi(VM_MIN_DIRECT_ADDRESS), tmp;   \
        mov SCRATCH_REG_HASH_KERNEL, reg;         \
	sllx tmp, 32, tmp;                        \
	ldxa [%g0 + reg]ASI_SCRATCHPAD, reg;      \
	andn reg, tmp, reg;



#define GET_TSB_SCRATCH_USER(reg)         \
        mov SCRATCH_REG_TSB_USER, reg;    \
	ldxa [%g0 + reg]ASI_SCRATCHPAD, reg;

#define GET_TSB_SCRATCH_KERNEL(reg)	  \
        mov SCRATCH_REG_TSB_KERNEL, reg;         \
	ldxa [%g0 + reg]ASI_SCRATCHPAD, reg; 

#define SET_SCRATCH(offsetreg, reg)   stxa reg, [%g0 + offsetreg]ASI_SCRATCHPAD


#define GET_PCB_PHYS(tmp, reg)                    \
        mov PC_CURPCB, reg;                       \
        GET_PCPU_PHYS_SCRATCH(tmp);               \
        ldxa [PCPU_REG + reg]ASI_REAL, reg;       \
        sub reg, tmp, reg;


#define GET_PCB(reg)	    \
        GET_PCPU_SCRATCH;   \
        ldx [PCPU_REG + PC_CURPCB], reg;

#define SET_MMU_CONTEXT(typereg, reg)     stxa reg, [typereg]ASI_MMU_CONTEXTID
#define GET_MMU_CONTEXT(typereg, reg)     ldxa [typereg]ASI_MMU_CONTEXTID, reg



#define	SAVE_GLOBALS(TF) \
	stx	%g1, [TF + TF_G1]; \
	stx	%g2, [TF + TF_G2]; \
	stx	%g3, [TF + TF_G3]; \
	stx	%g4, [TF + TF_G4]; \
	stx	%g5, [TF + TF_G5]; \
	stx	%g6, [TF + TF_G6]; 

#define	RESTORE_GLOBALS_USER(TF) \
	ldx	[TF + TF_G1], %g1; \
	ldx	[TF + TF_G2], %g2; \
	ldx	[TF + TF_G3], %g3; \
	ldx	[TF + TF_G4], %g4; \
	ldx	[TF + TF_G5], %g5; \
	ldx	[TF + TF_G6], %g6; \
	ldx	[TF + TF_G7], %g7;

#define	RESTORE_GLOBALS_KERNEL(TF) \
	mov  SCRATCH_REG_PCPU, %g7; \
	ldx	[TF + TF_G1], %g1; \
	ldx	[TF + TF_G2], %g2; \
	ldx	[TF + TF_G3], %g3; \
	ldx	[TF + TF_G4], %g4; \
	ldx	[TF + TF_G5], %g5; \
	ldx	[TF + TF_G6], %g6; \
	ldxa [%g0 + %g7]ASI_SCRATCHPAD, %g7;

#define	SAVE_OUTS(TF) \
	stx	%i0, [TF + TF_O0]; \
	stx	%i1, [TF + TF_O1]; \
	stx	%i2, [TF + TF_O2]; \
	stx	%i3, [TF + TF_O3]; \
	stx	%i4, [TF + TF_O4]; \
	stx	%i5, [TF + TF_O5]; \
	stx	%i6, [TF + TF_O6]; \
	stx	%i7, [TF + TF_O7];

#define	RESTORE_OUTS(TF) \
	ldx	[TF + TF_O0], %i0; \
	ldx	[TF + TF_O1], %i1; \
	ldx	[TF + TF_O2], %i2; \
	ldx	[TF + TF_O3], %i3; \
	ldx	[TF + TF_O4], %i4; \
	ldx	[TF + TF_O5], %i5; \
	ldx	[TF + TF_O6], %i6; \
	ldx	[TF + TF_O7], %i7;


#define	SAVE_WINDOW(SBP) \
	stx	%l0, [SBP + (0*8)]; \
	stx	%l1, [SBP + (1*8)]; \
	stx	%l2, [SBP + (2*8)]; \
	stx	%l3, [SBP + (3*8)]; \
	stx	%l4, [SBP + (4*8)]; \
	stx	%l5, [SBP + (5*8)]; \
	stx	%l6, [SBP + (6*8)]; \
	stx	%l7, [SBP + (7*8)]; \
	stx	%i0, [SBP + (8*8)]; \
	stx	%i1, [SBP + (9*8)]; \
	stx	%i2, [SBP + (10*8)]; \
	stx	%i3, [SBP + (11*8)]; \
	stx	%i4, [SBP + (12*8)]; \
	stx	%i5, [SBP + (13*8)]; \
	stx	%i6, [SBP + (14*8)]; \
	stx	%i7, [SBP + (15*8)];

#define	SAVE_WINDOW_ASI(SBP) \
	stxa	%l0, [SBP + (0*8)]%asi; \
	stxa	%l1, [SBP + (1*8)]%asi; \
	stxa	%l2, [SBP + (2*8)]%asi; \
	stxa	%l3, [SBP + (3*8)]%asi; \
	stxa	%l4, [SBP + (4*8)]%asi; \
	stxa	%l5, [SBP + (5*8)]%asi; \
	stxa	%l6, [SBP + (6*8)]%asi; \
	stxa	%l7, [SBP + (7*8)]%asi; \
	stxa	%i0, [SBP + (8*8)]%asi; \
	stxa	%i1, [SBP + (9*8)]%asi; \
	stxa	%i2, [SBP + (10*8)]%asi; \
	stxa	%i3, [SBP + (11*8)]%asi; \
	stxa	%i4, [SBP + (12*8)]%asi; \
	stxa	%i5, [SBP + (13*8)]%asi; \
	stxa	%i6, [SBP + (14*8)]%asi; \
	stxa	%i7, [SBP + (15*8)]%asi;

#define	SAVE_LOCALS_ASI(SBP) \
	stxa	%l0, [SBP + (0*8)]%asi; \
	stxa	%l1, [SBP + (1*8)]%asi; \
	stxa	%l2, [SBP + (2*8)]%asi; \
	stxa	%l3, [SBP + (3*8)]%asi; \
	stxa	%l4, [SBP + (4*8)]%asi; \
	stxa	%l5, [SBP + (5*8)]%asi; \
	stxa	%l6, [SBP + (6*8)]%asi; \
	stxa	%l7, [SBP + (7*8)]%asi; 

#define	RESTORE_LOCALS_ASI(SBP) \
	ldxa	[SBP + (0*8)]%asi, %l0;	\
	ldxa	[SBP + (1*8)]%asi, %l1;	\
	ldxa	[SBP + (2*8)]%asi, %l2;	\
	ldxa	[SBP + (3*8)]%asi, %l3;	\
	ldxa	[SBP + (4*8)]%asi, %l4;	\
	ldxa	[SBP + (5*8)]%asi, %l5;	\
	ldxa	[SBP + (6*8)]%asi, %l6;	\
	ldxa	[SBP + (7*8)]%asi, %l7;	

#define	SAVE_OUTS_ASI(SBP) \
	stxa	%o0, [SBP + (0*8)]%asi; \
	stxa	%o1, [SBP + (1*8)]%asi; \
	stxa	%o2, [SBP + (2*8)]%asi; \
	stxa	%o3, [SBP + (3*8)]%asi; \
	stxa	%o4, [SBP + (4*8)]%asi; \
	stxa	%o5, [SBP + (5*8)]%asi; \
	stxa	%o6, [SBP + (6*8)]%asi; \
	stxa	%o7, [SBP + (7*8)]%asi; 

#define	RESTORE_OUTS_ASI(SBP) \
	ldxa	[SBP + (0*8)]%asi, %o0;	\
	ldxa	[SBP + (1*8)]%asi, %o1;	\
	ldxa	[SBP + (2*8)]%asi, %o2;	\
	ldxa	[SBP + (3*8)]%asi, %o3;	\
	ldxa	[SBP + (4*8)]%asi, %o4;	\
	ldxa	[SBP + (5*8)]%asi, %o5;	\
	ldxa	[SBP + (6*8)]%asi, %o6;	\
	ldxa	[SBP + (7*8)]%asi, %o7;	


#define TTRACE_ADD_SAFE(SBP, arg0, arg1, arg2, arg3, arg4) \
        SAVE_OUTS_ASI(SBP);  \
        mov arg0, %o0; \
        mov arg1, %o1; \
        mov arg2, %o2; \
        mov arg3, %o3; \
        mov arg4, %o4; \
        RESTORE_OUTS_ASI(SBP); 


#endif /* LOCORE */

#endif /* _KERNEL */

#endif /* !_MACHINE_ASMACROS_H_ */
