/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * Copyright (c) 2012-2014 Michael Roe
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

/* XXXRW: Can I drop some of these? */
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpuregs.h>
#include <machine/pte.h>

#ifdef CPU_CHERI
#include <machine/cheriasm.h>
#include <machine/cherireg.h>
#endif

#include "assym.s"

/*
 * Software implementations of CCall, CReturn handlers for CHERI.
 *
 * The low-level CHERICCallVector exception handler, which has been relocated
 * to the MIPS exception vector table, jumps to either CHERICCall or
 * CHERICReturn running in the normal kernel address space.
 *
 * Notice that 'j' is used, implying that the kernel is in the 32-bit kernel
 * segment so that the target fits in the available immediate -- this is also
 * true of other FreeBSD exception handlers.
 */

	.set noreorder	/* Preserve nops, allow instructions in b-d slots. */

/*
 * CCall/CReturn low-level exception handler; this code must be position-
 * independent, as it will be relocated into the vector table.
 *
 * NB: Our CCall/CReturn will always step on $at, even if the operation fails,
 * in order to give us a bit more register space than just $k0 and $k1.
 *
 * XXXRW: If we had separate vectors for CCall and CReturn, we might be able
 * to pull off CReturn without branching to a larger function elsewhere in
 * memory, reducing overhead.  CCall remains pretty big, at least or now.
 */
VECTOR(CHERICCallVector, unknown)
        .set push
        .set noat
        CHERI_EXCEPTION_ENTER(k0)

	/*
	 * Determine whether this is a CCall or CReturn instruction.
	 *
	 * XXXRW: Panic if CGetCause returns something other than CALL/RETURN.
	 *
	 * XXXRW: Panic if not entering from userspace.
	 *
	 * XXXRW: Should we be clearing $k0 and $k1 before returning to
	 * userspace?  Should other exception handlers be doing it?
	 */
	CGetCause	k1
	REG_SRL		k1, 8
	andi		k1, k1, 0x1	/* CALL is odd; RETURN is even. */
	beqz		k1, CReturn_label
	nop		/* Branch-delay slot. */

	j		CHERICCall
	nop		/* Branch-delay slot. */

CReturn_label:
	j		CHERICReturn
	nop		/* Branch-delay slot. */

        .set pop
VECTOR_END(CHERICCallVector)

/*
 * Software implementation of CCall; this code does not need to be position-
 * independent as it is not relocated to an exception vector.
 *
 * XXXRW: This software implementation of CCall uses an alternative calling
 * convention, in which the code capability to invoke is always placed in $c1,
 * and the corresponding data capability is always placed in $c2.  This
 * prevents software from having to copy in the faulting ccall instruction,
 * decoding it, and then switch()ing on the register numbers to use the right
 * instruction to extract the capabilities to fixed ones targetable in
 * assembly and by the compiler.
 *
 * NB: No attempt to make this pipeline well yet -- branch-delay slots not
 * well-utilised, some CP2 fields accessed multiple times.  Also, CCall
 * provides hardware acceleration of some checks which we currently duplicate.
 * There are optimisation opportunities in terms of the ISA, especially as
 * relates to 'global' checking and general-purpose and capability register
 * clearing.
 */
CHERICCall:
        .set push
        .set noat

	/*
	 * The effective ABI requires that the code capability be in $c0, and
	 * the data capability be in $c1.  Throw an exception if this isn't
	 * how CCall was invoked.
	 */
	/*
	 * XXXRW: Not yet: can't get both register numbers from cause code.
	 * Once we can, we can remove all pre-trusted-stack tests from this
	 * handler.
	 */

	/* First, test argument registers for tag validity. */
	cbtu		CHERI_REG_CCALLCODE, CCall_c1_untagged
	nop
	cbtu		CHERI_REG_CCALLDATA, CCall_c2_untagged
	nop

	/* Second, check for the sealed bit on both arguments. */
	cgetsealed	k0, CHERI_REG_CCALLCODE
	beqz		k0, CCall_c1_unsealed
	nop

	cgetsealed	k0, CHERI_REG_CCALLDATA
	beqz		k0, CCall_c2_unsealed
	nop

	/* Third, check for type equality. */
	cgettype	k0, CHERI_REG_CCALLCODE
	cgettype	k1, CHERI_REG_CCALLDATA
	bne		k0, k1, CCall_c1_c2_type_mismatch
	nop

	/* Fourth, check permissions. */
	cgetperm	k0, CHERI_REG_CCALLCODE
	andi		k0, CHERI_PERM_EXECUTE
	beqz		k0, CCall_c1_perm_execute
	nop

	cgetperm	k0, CHERI_REG_CCALLDATA
	andi		k0, CHERI_PERM_EXECUTE
	bnez		k0, CCall_c2_perm_execute
	nop

	/* Fifth, check that PCC offset is not >= length. */
	/* XXXRW: Check that this is right! */
	cgetoffset	k0, CHERI_REG_CCALLDATA
	cgetlen		k1, CHERI_REG_CCALLDATA
	sltu		k1, k1, k0
	bnez		k1, CCall_c1_range
	nop

	/*
	 * Sixth: check that all argument capabilities with tags have the
	 * 'global' permission bit set.
	 *
	 * NB: We use $at here as we know that we will want to clear it later;
	 * this is a little naughty for an exception handler, but fine for
	 * CCall/CReturn.  This way we avoid lots of branches here, though.
	 *
	 * XXXRW: This is not very efficient using the current ISA; we might
	 * want some sort of 'set-if-not-global-and-also-not-null'
	 * instruction?  Then we could run down all the pertinent registers
	 * and simply branch once at the end.
	 */
	dli		AT, 1		/* Cleared if a failure. */

	cgettag		k0, $c1			/* k0 = tagged. */
	cgetperm	k1, $c1			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c2			/* k0 = tagged. */
	cgetperm	k1, $c2			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c3			/* k0 = tagged. */
	cgetperm	k1, $c3			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c4			/* k0 = tagged. */
	cgetperm	k1, $c4			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c5			/* k0 = tagged. */
	cgetperm	k1, $c5			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c6			/* k0 = tagged. */
	cgetperm	k1, $c6			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c7			/* k0 = tagged. */
	cgetperm	k1, $c7			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c8			/* k0 = tagged. */
	cgetperm	k1, $c8			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c9			/* k0 = tagged. */
	cgetperm	k1, $c9			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	cgettag		k0, $c10		/* k0 = tagged. */
	cgetperm	k1, $c10		/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	movn		AT, zero, k0		/* Clear if true. */

	/* One branch to rule them all. */
#if 0
	beqz		AT, CCall_local_argument
#else
	nop
#endif
	nop

	/*
	 * Now prepare to push IDC, PCC, PC+4 onto the trusted stack.  Begin
	 * by retrieving the current PCB pointer to reach the trusted stack.
	 */
	GET_CPU_PCPU(k1)   
	PTR_L		k1, PC_CURPCB(k1)

	/* Retrieve current trusted stack pointer. */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)

	/* If at bottom (byte offset 0), then overflow. */
	beqz		k0, CCall_stack_overflow
	nop

	/* Decrement trusted stack pointer. */
	PTR_SUBIU	k0, k0, CHERI_FRAME_SIZE

	/* Write back trusted stack pointer. */
	PTR_S		k0, U_PCB_CHERISTACK_TSP(k1)

	/* Convert trusted stack-relative offset to global pointer. */
	PTR_ADDU	k0, k1, k0			/* Add PCB pointer. */
	PTR_ADDIU	k0, k0, U_PCB_CHERISTACK_FRAMES	/* Add PCB offset. */

	/* Push IDC. */
	csc		CHERI_REG_IDC, k0, CHERI_STACKFRAME_IDC(CHERI_REG_KDC)

	/*
	 * Add 4 to PC, install in PC.offset; k1 is overwritten so no longer
	 * the PCB pointer.
	 *
	 * NB: It seems like the hardware could do this for us?
	 */
	MFC0		k1, MIPS_COP_0_EXC_PC
	PTR_ADDU	k1, k1, 4
	csetoffset	CHERI_REG_EPCC, CHERI_REG_EPCC, k1

	/* Push PCC. */
	csc		CHERI_REG_EPCC, k0, CHERI_STACKFRAME_PCC(CHERI_REG_KDC)

	/*
	 * Temporarily set KDC type to allow unsealing; this is a bit awkward
	 * as KDC can't be used for data access until we unset it again.
	 *
	 * NB: Note implicit assumption that CHERI_REG_KDC.base == 0.
	 *
	 * XXXRW: This is inefficient and (should be) unnecessary.
	 */
	cgettype	k0, CHERI_REG_CCALLCODE
	csetoffset	CHERI_REG_KDC, CHERI_REG_KDC, k0

	/* Unseal cs; install in PCC. */
	cunseal		CHERI_REG_EPCC, CHERI_REG_CCALLCODE, CHERI_REG_KDC

	/* Unseal cb; install in IDC. */
	cunseal		CHERI_REG_IDC, CHERI_REG_CCALLDATA, CHERI_REG_KDC

	/* Restore KDC offset. */
	dli		k0, 0
	csetoffset	CHERI_REG_KDC, CHERI_REG_KDC, k0

	/* Install cs.offset as PC. */
	cgetoffset	k0, CHERI_REG_CCALLCODE
	MTC0		k0, MIPS_COP_0_EXC_PC
	COP0_SYNC

	/*
	 * Clear non-argument registers.  The compiler (or runtime) is
	 * responsible for clearing unused argument registers to prevent
	 * leaks.
	 *
	 * XXXRW: There is a strong argument that this should be done in
	 * userspace to avoid encoding ABI choices about argument registers in
	 * this exception handler.  This would also leave choices about
	 * symmetric vs. asymmetric trust in userspace (and the hands of the
	 * compiler and runtime).  However, this also leaves both caller and
	 * callee with a need to clear overlapping sets of registers,
	 * increasing overall work.  As a result, we do it here (for now).
	 */

	/* Non-argument general-purpose registers (n64). */
	li		AT, 0
	li		v0, 0
	li		v1, 0
	/* $a0 handled by compiler. */
	/* $a1 handled by compiler. */
	/* $a2 handled by compiler. */
	/* $a3 handled by compiler. */
	/* $a4 handled by compiler. */
	/* $a5 handled by compiler. */
	/* $a6 handled by compiler. */
	/* $a7 handled by compiler. */
	li		t0, 0
	li		t1, 0
	li		t2, 0
	li		t3, 0
	li		s0, 0
	li		s1, 0
	li		s2, 0
	li		s3, 0
	li		s4, 0
	li		s5, 0
	li		s6, 0
	li		s7, 0
	li		t8, 0
	li		t9, 0
	li		gp, 0
	li		sp, 0
	li		s8, 0
	li		ra, 0
	mtlo		zero
	mthi		zero

	/* Non-argument capability registers. */
	cfromptr	$c0, $c0, zero
	/* $c1 preserved as the passed code capability. */
	/* $c2 preserved as the passed data capability. */
	/* $c3 handled by compiler; global bit checked earlier. */
	/* $c4 handled by compiler; global bit checked earlier. */
	/* $c5 handled by compiler; global bit checked earlier. */
	/* $c6 handled by compiler; global bit checked earlier. */
	/* $c7 handled by compiler; global bit checked earlier. */
	/* $c8 handled by compiler; global bit checked earlier. */
	/* $c9 handled by compiler; global bit checked earlier. */
	/* $c10 handled by compiler; global bit checked earlier. */
	cfromptr	$c11, $c11, zero
	cfromptr	$c12, $c12, zero
	cfromptr	$c13, $c13, zero
	cfromptr	$c14, $c14, zero
	cfromptr	$c15, $c15, zero
	cfromptr	$c16, $c16, zero
	cfromptr	$c17, $c17, zero
	cfromptr	$c18, $c18, zero
	cfromptr	$c19, $c19, zero
	cfromptr	$c20, $c20, zero
	cfromptr	$c21, $c21, zero
	cfromptr	$c22, $c22, zero
	cfromptr	$c23, $c23, zero
	cfromptr	$c24, $c24, zero
	cfromptr	$c25, $c25, zero

	CHERI_EXCEPTION_RETURN(k0)
	eret

CCall_c1_untagged:
	dli	k0, ((CHERI_EXCCODE_TAG << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c2_untagged:
	dli	k0, ((CHERI_EXCCODE_TAG << 8) | 2)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_unsealed:
	dli	k0, ((CHERI_EXCCODE_SEAL << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c2_unsealed:
	dli	k0, ((CHERI_EXCCODE_SEAL << 8) | 2)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_c2_type_mismatch:
	dli	k0, ((CHERI_EXCCODE_TYPE << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_perm_execute:
	dli	k0, ((CHERI_EXCCODE_PERM_EXECUTE << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c2_perm_execute:
	dli	k0, ((CHERI_EXCCODE_PERM_EXECUTE << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_range:
	dli	k0, ((CHERI_EXCCODE_LENGTH << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_local_argument:
	dli	k0, ((CHERI_EXCCODE_SW_LOCALARG << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_stack_overflow:
	dli	k0, ((CHERI_EXCCODE_CALL << 8) | 0)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_throw_exception:
	j	_C_LABEL(MipsUserGenException)
	nop

        .set pop

/*
 * Software implementation of CReturn; this code does not need to be position-
 * independent as it is not relocated to an exception vector.
 *
 * Possible failure modes:
 *
 * 1. Attempt to return a non-global capability.
 * 2. Trusted stack underflow.
 */
CHERICReturn:
        .set push
        .set noat

	/* Check that return capability is global or untagged. */
	cgettag		k0, $c3			/* k0 = tagged. */
	cgetperm	k1, $c3			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
#if 0
	bnez		k1, CReturn_local_retval
#else
	nop
#endif
	nop

	/* Retrieve current PCB pointer. */
	GET_CPU_PCPU(k1)   
	PTR_L		k1, PC_CURPCB(k1)

	/*
	 * The only currently defined check in CReturn is stack underflow;
	 * perform that check.
	 */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)
	sltiu		k0, CHERI_STACK_SIZE
	beqz		k0, CReturn_stack_underflow
	nop

	/*
	 * Reload trusted stack pointer.
	 *
	 * XXXRW: Actually, we could avoid this by borrowing a to-be-cleared
	 * user register, such as $at, rather than juggling just $k0/$k1.
	 */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)
	
	/* Convert trusted stack-relative offset to global pointer. */
	PTR_ADDU	k0, k1, k0			/* Add PCB pointer. */
	PTR_ADDIU	k0, k0, U_PCB_CHERISTACK_FRAMES	/* Add PCB offset. */

	/* Pop IDC. */
	clc		CHERI_REG_IDC, k0, CHERI_STACKFRAME_IDC(CHERI_REG_KDC)

	/* Pop PCC. */
	clc		CHERI_REG_EPCC, k0, CHERI_STACKFRAME_PCC(CHERI_REG_KDC)

	/* Extract PC+4, install in $c0_epc.  Toasts k0; k1 still PCB. */
	cgetoffset	k0, CHERI_REG_EPCC
	MTC0		k0, MIPS_COP_0_EXC_PC
	COP0_SYNC

	/* Update trusted stack pointer. */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)
	PTR_ADDIU	k0, CHERI_FRAME_SIZE
	PTR_S		k0, U_PCB_CHERISTACK_TSP(k1)

	/*
	 * Clear non-return-value registers.  The compiler (or runtime) is
	 * responsible for clearing unused return-value registers to prevent
	 * leaks.
	 *
	 * XXXRW: See note above for CCall-side register clearing for comments
	 * on why this isn't necessarily the right thing, but is what we do in
	 * practice.
	 */

	/* Non-return-value general-purpose registers (n64). */
	li		AT, 0
	/* v0 handled by compiler. */
	/* v1 handled by compiler. */
	li		a0, 0
	li		a1, 0
	li		a2, 0
	li		a3, 0
	li		a4, 0
	li		a5, 0
	li		a6, 0
	li		a7, 0
	li		t0, 0
	li		t1, 0
	li		t2, 0
	li		t3, 0
	li		s0, 0
	li		s1, 0
	li		s2, 0
	li		s3, 0
	li		s4, 0
	li		s5, 0
	li		s6, 0
	li		s7, 0
	li		t8, 0
	li		t9, 0
	li		gp, 0
	li		sp, 0
	li		s8, 0
	li		ra, 0
	mtlo		zero
	mthi		zero

	/* Non-return-value capability registers. */
	cfromptr	$c0, $c0, zero
	cfromptr	$c1, $c1, zero
	cfromptr	$c2, $c2, zero
	/* $c3 handled by compiler; global bit checked earlier. */
	cfromptr	$c4, $c4, zero
	cfromptr	$c5, $c5, zero
	cfromptr	$c6, $c6, zero
	cfromptr	$c7, $c7, zero
	cfromptr	$c8, $c8, zero
	cfromptr	$c9, $c9, zero
	cfromptr	$c10, $c10, zero
	cfromptr	$c11, $c11, zero
	cfromptr	$c12, $c12, zero
	cfromptr	$c13, $c13, zero
	cfromptr	$c14, $c14, zero
	cfromptr	$c15, $c15, zero
	cfromptr	$c16, $c16, zero
	cfromptr	$c17, $c17, zero
	cfromptr	$c18, $c18, zero
	cfromptr	$c19, $c19, zero
	cfromptr	$c20, $c20, zero
	cfromptr	$c21, $c21, zero
	cfromptr	$c22, $c22, zero
	cfromptr	$c23, $c23, zero
	cfromptr	$c24, $c24, zero
	cfromptr	$c25, $c25, zero
	CHERI_EXCEPTION_RETURN(k0)
	eret

CReturn_local_retval:
	dli	k0, ((CHERI_EXCCODE_SW_LOCALRET << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CReturn_stack_underflow:
	dli	k0, ((CHERI_EXCCODE_RETURN << 8) | 0)
	csetcause	k0
	j	_C_LABEL(MipsUserGenException)
	nop

	.set pop
