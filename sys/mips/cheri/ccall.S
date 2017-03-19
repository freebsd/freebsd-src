/*-
 * Copyright (c) 2012-2017 Robert N. M. Watson
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

#include "opt_ktrace.h"

/* XXXRW: Can I drop some of these? */
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpuregs.h>
#include <machine/exceptionasm.h>
#include <machine/pte.h>
#include <machine/pcb.h>

#ifdef CPU_CHERI
#include <machine/cheriasm.h>
#include <machine/cherireg.h>
#endif

#ifndef KTR_CCALL
#define KTR_CCALL	15
#endif
#ifndef KTRFAC_CCALL
#define	KTRFAC_CCALL	(1<<KTR_CCALL)
#endif
#ifndef KTR_CRETURN
#define KTR_CRETURN	16
#endif
#ifndef KTRFAC_CRETURN
#define	KTRFAC_CRETURN	(1<<KTR_CCALL)
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
 * NB: Our CCall/CReturn will always step on $at, and CCall will treat on $t0,
 * even if the operation fails, in order to give us a bit more register space
 * than just $k0 and $k1.
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

	/* Increment the exception counter for ccall, if enabled. */
ccall_inc_exception_cnt:
	INC_EXCEPTION_CNTR(CHERI_CCALL_CNT)

	/*
	 * The effective ABI requires that the code capability be in $c1, and
	 * the data capability be in $c2.  Throw an exception if this isn't
	 * how CCall was invoked.
	 */
	/*
	 * XXXRW: Not yet: can't get both register numbers from cause code.
	 * Once we can, we can remove all pre-trusted-stack tests from this
	 * handler.
	 */
ccall_validate_args:
#if !defined(CPU_CHERI_HW_CCALL_CHECKS)
	/* First, test argument registers for tag validity. */
	cbtu		CHERI_REG_CCALLCODE, CCall_c1_untagged
	nop
	cbtu		CHERI_REG_CCALLDATA, CCall_c2_untagged
	/* NB branch delay utilised below */

	/* Second, check for the sealed bit on both arguments. */
	cgetsealed	k0, CHERI_REG_CCALLCODE
	beqz		k0, CCall_c1_unsealed
	/* NB branch delay utilised below */

	cgetsealed	k0, CHERI_REG_CCALLDATA
	beqz		k0, CCall_c2_unsealed
	/* NB branch delay utilised below */

	/* Third, check for type equality. */
	cgettype	t0, CHERI_REG_CCALLCODE
	cgettype	k1, CHERI_REG_CCALLDATA
	bne		t0, k1, CCall_c1_c2_type_mismatch
	/* NB branch delay utilised below */

	/* Fourth, check permissions. */
	cgetperm	k0, CHERI_REG_CCALLCODE
	andi		k0, CHERI_PERM_EXECUTE
	beqz		k0, CCall_c1_perm_execute
	/* NB branch delay utilised below */

	cgetperm	k0, CHERI_REG_CCALLDATA
	andi		k0, CHERI_PERM_EXECUTE
	bnez		k0, CCall_c2_perm_execute
	/* NB branch delay utilised below */

	/* Fifth, check that $pcc offset is not >= length. */
	/* XXXRW: Check that this is right! */
	cgetoffset	k0, CHERI_REG_CCALLDATA
	cgetlen		k1, CHERI_REG_CCALLDATA
	sltu		k1, k1, k0
	bnez		k1, CCall_c1_range
	nop
#else /* CPU_CHERI_HW_CCALL_CHECKS */
	/*
	 * When the processor performs checks for tags, sealing, type,
	 * permissions, and range on registers, it does so for argument
	 * registers specified in the instruction.  We must check that they
	 * conform with our $c1/$c2 calling convention or we risk the checks
	 * having been performed on the wrong registers.  We do this by
	 * inspecting the exception-instruction CP0 register.
	 */
	dmfc0		k0, MIPS_COP_0_EXC_INS, 1	/* Get ins. encoding */
	srl		k0, k0, 11	/* Shift register numbers to bottom */
	and		k0, k0, 0x3ff	/* Mask everything but reg. numbers */
	li		k1, ((1<<5) | 2)/* Check that arguments are c1 and c2 */
	bne		k0, k1, CCall_wrong_regs
	cgettype		t0, CHERI_REG_CCALLCODE
#endif /* CPU_CHERI_HW_CCALL_CHECKS */

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

	/*
	 * t0 contains the type for the sealed capability on entry to this
	 * block, either as a side effect of the software checks or from the
	 * delay slot in the branch for hardware check failures.
	 */
#ifdef WORKING_dmfc0_8_1
	dmfc0		k0, _(8), 1	/* Get ins. encoding */
	and		k0, k0, 0x7FF	/* Get the selector */
	move		t0, t0
	srl		t0, t0, 22	/* Is this a system capability? */
	bne		t0, k0, CCall_wrong_cc/* If it is, then the selector must be 1. */
	dli		AT, 0	/* Set if failure; will be regnum.
				   (hoisted from ccall_check_globals,
				   ignored if branch is taken) */
	bnez		t0, ccall_skip_global_check /* Skip global checks if
					   it's a system capability */
#else
	srl		t0, t0, 22	/* Is this a system capability? */
	bnez		t0, ccall_skip_global_check /* Skip global checks if
					   it's a system capability */
	dli		AT, 0	/* Set if failure; will be regnum.
				   (hoisted from ccall_check_globals,
				   ignored if branch is taken) */
#endif
	/* Delay slot snaffles the next instruction.  It's cheap and we don't
	 * use the register on the branch-taken path. */
ccall_check_globals:
	cgettag		k0, $c1			/* k0 = tagged. */
	cgetperm	k1, $c1			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 1			/* $c1 */
	movn		AT, t0, k0		/* Clear if true. */

	cgettag		k0, $c2			/* k0 = tagged. */
	cgetperm	k1, $c2			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 2			/* $c2 */
	movn		AT, t0, k0		/* Clear if true. */

	cgettag		k0, $c3			/* k0 = tagged. */
	cgetperm	k1, $c3			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 3			/* $c3 */
	movn		AT, t0, k0		/* Clear if true. */

#if !defined(CPU_CHERI_CHERI8)
	cgettag		k0, $c4			/* k0 = tagged. */
	cgetperm	k1, $c4			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 4			/* $c4 */
	movn		AT, t0, k0		/* Clear if true. */

	cgettag		k0, $c5			/* k0 = tagged. */
	cgetperm	k1, $c5			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 5			/* $c5 */
	movn		AT, t0, k0		/* Clear if true. */

	cgettag		k0, $c6			/* k0 = tagged. */
	cgetperm	k1, $c6			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 6			/* $c6 */
	movn		AT, t0, k0		/* Clear if true. */

	cgettag		k0, $c7			/* k0 = tagged. */
	cgetperm	k1, $c7			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 7			/* $c7 */
	movn		AT, t0, k0		/* Clear if true. */
#endif
#if !defined(CPU_CHERI_CHERI8) && !defined(CHERI_CPU_CHERI16)
	cgettag		k0, $c8			/* k0 = tagged. */
	cgetperm	k1, $c8			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 8			/* $c8 */
	movn		AT, t0, k0		/* Clear if true. */

	cgettag		k0, $c9			/* k0 = tagged. */
	cgetperm	k1, $c9			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 9			/* $c9 */
	movn		AT, t0, k0		/* Clear if true. */

	cgettag		k0, $c10		/* k0 = tagged. */
	cgetperm	k1, $c10		/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	dli		t0, 10			/* $c10 */
	movn		AT, t0, k0		/* Clear if true. */
#endif
	/* One branch to rule them all. */
	bnez		AT, CCall_local_argument
	nop

	/*
	 * Now prepare to push $idc, $pcc, $pc+4 onto the trusted stack.
	 * Begin by retrieving the current PCB pointer to reach the trusted
	 * stack.
	 */
ccall_skip_global_check:
ccall_push_stack:
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

	/* Push $idc. */
	csc		CHERI_REG_IDC, k0, CHERI_STACKFRAME_IDC(CHERI_REG_KDC)

	/*
	 * Add 4 to $pc, install in $pcc.offset; k1 is overwritten so no
	 * longer the PCB pointer.
	 *
	 * NB: It seems like the hardware could do this for us?
	 */
	MFC0		k1, MIPS_COP_0_EXC_PC
	PTR_ADDU	k1, k1, 4
	csetoffset	CHERI_REG_EPCC, CHERI_REG_EPCC, k1

	/* Push $pcc. */
	csc		CHERI_REG_EPCC, k0, CHERI_STACKFRAME_PCC(CHERI_REG_KDC)

#ifdef KTRACE
ccall_maybe_ktrace:
	GET_CPU_PCPU(k1)
	PTR_L		k0, PC_CURTHREAD(k1)
	PTR_L		k0, TD_PROC(k0)
	INT_L		k0, PROC_TRACEFLAG(k0)
	andi		k0, KTRFAC_CCALL
	beqz		k0, ccall_unseal_caps
	nop

	PTR_LA		k0, _C_LABEL(CHERICCallKtrace)
	jalr		k0
	nop
#endif /* KTRACE */

ccall_unseal_caps:
	/*
	 * Unseal the sealed code and data capability operands by constructing
	 * a suitably authorised capability in $kr1c.  The ISA has already
	 * checked that the two types match.
	 */
	cgettype	k0, CHERI_REG_CCALLCODE
	csetoffset	CHERI_REG_KR1C, CHERI_REG_KDC, k0
	cgetoffset	k0, CHERI_REG_CCALLCODE # moved from below to avoid delay on cheri128

	/* Unseal cs; install in $pcc. */
	cunseal		CHERI_REG_EPCC, CHERI_REG_CCALLCODE, CHERI_REG_KR1C

	/* Unseal cb; install in $idc. */
	cunseal		CHERI_REG_IDC, CHERI_REG_CCALLDATA, CHERI_REG_KR1C

	/* Install cs.offset as $pc. */
ccall_set_pc:
	#cgetoffset	k0, CHERI_REG_CCALLCODE # moved above to avoid delay on cheri128
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
ccall_clear_regs:
#if !defined(CPU_CHERI_HW_RCLEAR)
	li		AT, 0
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
#else /* CPU_CHERI_HW_RCLEAR */
	CHERI_CLEAR_GPLO16( \
		CHERI_CLEAR_GPLO_AT | \
		CHERI_CLEAR_GPLO_V1 | \
		CHERI_CLEAR_GPLO_T0 | \
		CHERI_CLEAR_GPLO_T1 | \
		CHERI_CLEAR_GPLO_T2 | \
		CHERI_CLEAR_GPLO_T3 )
	CHERI_CLEAR_GPHI16( \
		CHERI_CLEAR_GPHI_S0 | \
		CHERI_CLEAR_GPHI_S1 | \
		CHERI_CLEAR_GPHI_S2 | \
		CHERI_CLEAR_GPHI_S3 | \
		CHERI_CLEAR_GPHI_S4 | \
		CHERI_CLEAR_GPHI_S5 | \
		CHERI_CLEAR_GPHI_S6 | \
		CHERI_CLEAR_GPHI_S7 | \
		CHERI_CLEAR_GPHI_T8 | \
		CHERI_CLEAR_GPHI_T9 | \
		CHERI_CLEAR_GPHI_GP | \
		CHERI_CLEAR_GPHI_SP | \
		CHERI_CLEAR_GPHI_S8 | \
		CHERI_CLEAR_GPHI_RA )
#endif /* CPU_CHERI_HW_RCLEAR */
	mtlo		zero
	mthi		zero

ccall_clear_caps:
#if !defined(CPU_CHERI_HW_CCLEAR)
	/* Non-argument capability registers. */

	/* $ddc has been set aside in $kr2c, so clear it there. */
	cfromptr	CHERI_REG_SEC0, CHERI_REG_SEC0, zero
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
#if  !defined(CPU_CHERI_CHERI8) && !defined(CHERI_CPU_CHERI16)
	cfromptr	$c13, $c13, zero
	cfromptr	$c14, $c14, zero
	cfromptr	$c15, $c15, zero
	cfromptr	$c16, $c16, zero
#endif
	cfromptr	$c17, $c17, zero
	cfromptr	$c18, $c18, zero
	cfromptr	$c19, $c19, zero
#if !defined(CPU_CHERI_CHERI8)
	cfromptr	$c20, $c20, zero
	cfromptr	$c21, $c21, zero
	cfromptr	$c22, $c22, zero
	cfromptr	$c23, $c23, zero
#endif
#if  !defined(CPU_CHERI_CHERI8) && !defined(CHERI_CPU_CHERI16)
	cfromptr	$c24, $c24, zero
	cfromptr	$c25, $c25, zero
#endif

#else /* CPU_CHERI_HW_CCLEAR */
	/* $ddc has been set aside in $kr2c, so clear it there. */
	CHERI_CLEAR_CAPLO16( \
		CHERI_CLEAR_CAPLO_C11 | \
		CHERI_CLEAR_CAPLO_C12 | \
		CHERI_CLEAR_CAPLO_C13 | \
		CHERI_CLEAR_CAPLO_C14 | \
		CHERI_CLEAR_CAPLO_C15 )
	CHERI_CLEAR_CAPHI16( \
		CHERI_CLEAR_CAPHI_C16 | \
		CHERI_CLEAR_CAPHI_C17 | \
		CHERI_CLEAR_CAPHI_C18 | \
		CHERI_CLEAR_CAPHI_C19 | \
		CHERI_CLEAR_CAPHI_C20 | \
		CHERI_CLEAR_CAPHI_C21 | \
		CHERI_CLEAR_CAPHI_C22 | \
		CHERI_CLEAR_CAPHI_C23 | \
		CHERI_CLEAR_CAPHI_C24 | \
		CHERI_CLEAR_CAPHI_C25 | \
		CHERI_CLEAR_CAPHI_SEC0 )
#endif /* CPU_CHERI_HW_CCLEAR */
ccall_exception_return:
	CHERI_EXCEPTION_RETURN(k0)
	eret

#if !defined(CPU_CHERI_HW_CCALL_CHECKS)

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

#else /* CPU_CHERI_HW_CCALL_CHECKS */

CCall_wrong_regs:
	dli	k0, ((CHERI_EXCCODE_SW_CCALLREGS << 8) | 0)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

#endif /* CPU_CHERI_HW_CCALL_CHECKS */

CCall_wrong_cc:
	dli	k0, ((CHERI_EXCCODE_SW_CCALLREGS << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_local_argument:
	dli	k0, (CHERI_EXCCODE_SW_LOCALARG << 8)
	or	k0, k0, AT
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_stack_overflow:
	dli	k0, ((CHERI_EXCCODE_SW_OVERFLOW << 8) | 0)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_throw_exception:
	j	_C_LABEL(MipsUserGenException)
	nop

	.set	pop

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
creturn_check_global:
	cgettag		k0, $c3			/* k0 = tagged. */
	cgetperm	k1, $c3			/* k1 = permissions. */
	andi		k1, CHERI_PERM_GLOBAL	/* k1 = global. */
	not		k1, k1			/* k1 = !global. */
	and		k0, k0, k1		/* k0 = tagged && !global */
	bnez		k0, CReturn_local_retval
	nop

	/* Increment the exception counter for creturn, if enabled. */
creturn_inc_exception_cnt:
	INC_EXCEPTION_CNTR(CHERI_CRETURN_CNT)

#ifdef KTRACE
creturn_maybe_ktrace:
	GET_CPU_PCPU(k1)   		/* XXXss Redundant if
					   INC_EXCEPTION_CNTR is enabled. */
	PTR_L		k0, PC_CURTHREAD(k1)
	PTR_L		k0, TD_PROC(k0)
	INT_L		k0, PROC_TRACEFLAG(k0)
	andi		k0, KTRFAC_CRETURN
	beqz		k0, creturn_pop_stack
	nop

	PTR_LA		k0, _C_LABEL(CHERICReturnKtrace)
	jalr		k0
	nop
#endif

	/* Retrieve current PCB pointer. */
creturn_pop_stack:
	GET_CPU_PCPU(k1)
	PTR_L		k1, PC_CURPCB(k1)

	/*
	 * The only currently defined check in CReturn is stack underflow;
	 * perform that check.
	 */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)
	sltiu		k0, k0, CHERI_STACK_SIZE
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

	/* Pop $idc. */
	clc		CHERI_REG_IDC, k0, CHERI_STACKFRAME_IDC(CHERI_REG_KDC)

	/* Pop $pcc. */
	clc		CHERI_REG_EPCC, k0, CHERI_STACKFRAME_PCC(CHERI_REG_KDC)

	/* Extract $pc+4, install in $c0_epc.  Toasts k0; k1 still PCB. */
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

creturn_clear_regs:
#if !defined(CPU_CHERI_HW_RCLEAR)
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
#else /* CPU_CHERI_HW_CCALL_CHECKS */
	/* Hypothesise that clearregs would take two instructions. */
	CHERI_CLEAR_GPLO16( \
		CHERI_CLEAR_GPLO_AT | \
		CHERI_CLEAR_GPLO_A0 | \
		CHERI_CLEAR_GPLO_A1 | \
		CHERI_CLEAR_GPLO_A2 | \
		CHERI_CLEAR_GPLO_A3 | \
		CHERI_CLEAR_GPLO_A4 | \
		CHERI_CLEAR_GPLO_A5 | \
		CHERI_CLEAR_GPLO_A6 | \
		CHERI_CLEAR_GPLO_A7 | \
		CHERI_CLEAR_GPLO_T0 | \
		CHERI_CLEAR_GPLO_T1 | \
		CHERI_CLEAR_GPLO_T2 | \
		CHERI_CLEAR_GPLO_T3 )
	CHERI_CLEAR_GPHI16( \
		CHERI_CLEAR_GPHI_S0 | \
		CHERI_CLEAR_GPHI_S1 | \
		CHERI_CLEAR_GPHI_S2 | \
		CHERI_CLEAR_GPHI_S3 | \
		CHERI_CLEAR_GPHI_S4 | \
		CHERI_CLEAR_GPHI_S5 | \
		CHERI_CLEAR_GPHI_S6 | \
		CHERI_CLEAR_GPHI_S7 | \
		CHERI_CLEAR_GPHI_T8 | \
		CHERI_CLEAR_GPHI_T9 | \
		CHERI_CLEAR_GPHI_GP | \
		CHERI_CLEAR_GPHI_SP | \
		CHERI_CLEAR_GPHI_S8 | \
		CHERI_CLEAR_GPHI_RA )
#endif /* CPU_CHERI_HW_CCALL_CHECKS */
	mtlo		zero
	mthi		zero

creturn_clear_caps:
#if !defined(CPU_CHERI_HW_CCLEAR)
	/* Non-return-value capability registers. */

	/* $ddc has been set aside in $kr2c, so clear it there. */
	cfromptr	CHERI_REG_SEC0, CHERI_REG_SEC0, zero
	cfromptr	$c1, $c1, zero
	cfromptr	$c2, $c2, zero
	/* $c3 handled by compiler; global bit checked earlier. */
#if !defined(CPU_CHERI_CHERI8)
	cfromptr	$c4, $c4, zero
	cfromptr	$c5, $c5, zero
	cfromptr	$c6, $c6, zero
	cfromptr	$c7, $c7, zero
#endif
#if !defined(CPU_CHERI_CHERI8) && !defined(CHERI_CPU_CHERI16)
	cfromptr	$c8, $c8, zero
	cfromptr	$c9, $c9, zero
	cfromptr	$c10, $c10, zero
#endif
	cfromptr	$c11, $c11, zero
	cfromptr	$c12, $c12, zero
#if !defined(CPU_CHERI_CHERI8) && !defined(CHERI_CPU_CHERI16)
	cfromptr	$c13, $c13, zero
	cfromptr	$c14, $c14, zero
	cfromptr	$c15, $c15, zero
	cfromptr	$c16, $c16, zero
#endif
	cfromptr	$c17, $c17, zero
	cfromptr	$c18, $c18, zero
	cfromptr	$c19, $c19, zero
#if !defined(CPU_CHERI_CHERI8)
	cfromptr	$c20, $c20, zero
	cfromptr	$c21, $c21, zero
	cfromptr	$c22, $c22, zero
	cfromptr	$c23, $c23, zero
#endif
#if !defined(CPU_CHERI_CHERI8) && !defined(CHERI_CPU_CHERI16)
	cfromptr	$c24, $c24, zero
	cfromptr	$c25, $c25, zero
#endif
#else
	/* $ddc has been set aside in $kr2c, so clear it there. */
	CHERI_CLEAR_CAPLO16( \
		CHERI_CLEAR_CAPLO_C1  | \
		CHERI_CLEAR_CAPLO_C2  | \
		CHERI_CLEAR_CAPLO_C4  | \
		CHERI_CLEAR_CAPLO_C5  | \
		CHERI_CLEAR_CAPLO_C6  | \
		CHERI_CLEAR_CAPLO_C7  | \
		CHERI_CLEAR_CAPLO_C8  | \
		CHERI_CLEAR_CAPLO_C9  | \
		CHERI_CLEAR_CAPLO_C10 | \
		CHERI_CLEAR_CAPLO_C11 | \
		CHERI_CLEAR_CAPLO_C12 | \
		CHERI_CLEAR_CAPLO_C13 | \
		CHERI_CLEAR_CAPLO_C14 | \
		CHERI_CLEAR_CAPLO_C15 )
	CHERI_CLEAR_CAPHI16( \
		CHERI_CLEAR_CAPHI_C16 | \
		CHERI_CLEAR_CAPHI_C17 | \
		CHERI_CLEAR_CAPHI_C18 | \
		CHERI_CLEAR_CAPHI_C19 | \
		CHERI_CLEAR_CAPHI_C20 | \
		CHERI_CLEAR_CAPHI_C21 | \
		CHERI_CLEAR_CAPHI_C22 | \
		CHERI_CLEAR_CAPHI_C23 | \
		CHERI_CLEAR_CAPHI_C24 | \
		CHERI_CLEAR_CAPHI_C25 | \
		CHERI_CLEAR_CAPHI_SEC0 )
#endif
creturn_exception_return:
	CHERI_EXCEPTION_RETURN(k0)
	eret

CReturn_local_retval:
	dli	k0, ((CHERI_EXCCODE_SW_LOCALRET << 8) | 3)
	csetcause	k0
	j	_C_LABEL(MipsUserGenException)
	nop

CReturn_stack_underflow:
	dli	k0, ((CHERI_EXCCODE_SW_UNDERFLOW << 8) | 0)
	csetcause	k0
	j	_C_LABEL(MipsUserGenException)
	nop

	.set pop
