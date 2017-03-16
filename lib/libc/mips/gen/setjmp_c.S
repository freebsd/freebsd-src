/*-
 * Copyright (c) 2015 SRI International
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");
#include <machine/regnum.h>

#include "SYS.h"

#ifdef __ABICALLS__
	.abicalls
#endif

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and a struct sigcontext, see <signal.h>
 */

#define SETJMP_FRAME_SIZE	(CALLFRAME_SIZ + _MIPS_SZCAP/8)

NESTED(setjmp, SETJMP_FRAME_SIZE, ra)
	.protected _C_LABEL(setjmp)
	.mask	0xc0000000, (CALLFRAME_RA - CALLFRAME_SIZ)
	PTR_SUBU sp, sp, SETJMP_FRAME_SIZE	# allocate stack frame

	csd		ra, sp, CALLFRAME_RA($c11)
	csd		s8, sp, CALLFRAME_FP($c11)
	csc		$c17, sp, CALLFRAME_C17($c11)
	csc		$c3, sp, CALLFRAME_SIZ($c11)	# store env
	move		s8, sp

	li		t0, (SZREG * 96)
	csetbounds	$c3, $c3, t0

	/* Get the signal mask. */
	li		t0, _JB_SIGMASK * SZREG
	csetoffset	$c3, $c3, t0			# oset
	cfromptr	$c4, $c3, zero			# set == NULL
	li		a0, 1				# SIG_SETBLOCK
	PIC_CALL(__cheriabi_sigprocmask)			# get current signal mask

	move		sp, s8

	clc		$c3, sp, CALLFRAME_SIZ($c11)	# restore env pointer
	clc		$c17, sp, CALLFRAME_C17($c11)	# restore return cap
	cld		s8, sp, CALLFRAME_FP($c11)
	cld		ra, sp, CALLFRAME_RA($c11)	# restore RA
	PTR_ADDU	sp, sp, SETJMP_FRAME_SIZE	# pop stack frame

	REG_LI		v0, _JB_MAGIC_SETJMP
	csd		v0, zero, (_JB_MAGIC  * SZREG)($c3)
	csd		ra, zero, (_JB_REG_RA * SZREG)($c3)
	/*
	 * From "MIPSpro N32 ABI Handbook", Table 2-1:
	 * Registers s0..s7 are callee-saved.
	 * The sp register is callee-saved.
	 * The fp (or s8) register is callee-saved.
	 * The gp register is callee-saved (for n32/n64).
	 */
	csd		s0, zero, (_JB_REG_S0 * SZREG)($c3)
	csd		s1, zero, (_JB_REG_S1 * SZREG)($c3)
	csd		s2, zero, (_JB_REG_S2 * SZREG)($c3)
	csd		s3, zero, (_JB_REG_S3 * SZREG)($c3)
	csd		s4, zero, (_JB_REG_S4 * SZREG)($c3)
	csd		s5, zero, (_JB_REG_S5 * SZREG)($c3)
	csd		s6, zero, (_JB_REG_S6 * SZREG)($c3)
	csd		s7, zero, (_JB_REG_S7 * SZREG)($c3)
	csd		sp, zero, (_JB_REG_SP * SZREG)($c3)
	csd		s8, zero, (_JB_REG_S8 * SZREG)($c3)
	csd		gp, zero, (_JB_REG_GP * SZREG)($c3)
#ifndef __mips_soft_float
	/*
	 * From "MIPSpro N32 ABI Handbook", Table 2-1:
	 * In N64, FP registers F24 .. F31 are callee-saved.
	 */
	cfc1	v0, $31
	csw	v0, zero, (_JB_FPREG_FCSR * SZREG)($c3)
	csdc1	$f24, zero, (_JB_FPREG_F24 * SZREG)($c3)
	csdc1	$f25, zero, (_JB_FPREG_F25 * SZREG)($c3)
	csdc1	$f26, zero, (_JB_FPREG_F26 * SZREG)($c3)
	csdc1	$f27, zero, (_JB_FPREG_F27 * SZREG)($c3)
	csdc1	$f28, zero, (_JB_FPREG_F28 * SZREG)($c3)
	csdc1	$f29, zero, (_JB_FPREG_F29 * SZREG)($c3)
	csdc1	$f30, zero, (_JB_FPREG_F30 * SZREG)($c3)
	csdc1	$f31, zero, (_JB_FPREG_F31 * SZREG)($c3)
#endif	/* ! __mips_soft_float */
	/* jmp_buf is 64-bit aligned, so we need to round it up. */
	cgetbase	t0, $c3
	cgetoffset	t1, $c3
	daddu		t1, t0, t1
	daddiu		t0, t1, ((_JB_CHERI_START * SZREG) + _MIPS_SZCAP/8 - SZREG)
	li		t2, _MIPS_CAP_ALIGN_MASK
	and		t2, t2, t0
	dsubu		t1, t2, t1

	csc		$c11, t1, (_JB_CHERI_C11 * _MIPS_SZCAP/8)($c3)
	csc		$c12, t1, (_JB_CHERI_C12 * _MIPS_SZCAP/8)($c3)
	csc		$c13, t1, (_JB_CHERI_C13 * _MIPS_SZCAP/8)($c3)
	csc		$c14, t1, (_JB_CHERI_C14 * _MIPS_SZCAP/8)($c3)
	csc		$c15, t1, (_JB_CHERI_C15 * _MIPS_SZCAP/8)($c3)
	csc		$c16, t1, (_JB_CHERI_C16 * _MIPS_SZCAP/8)($c3)
	csc		$c17, t1, (_JB_CHERI_C17 * _MIPS_SZCAP/8)($c3)
	csc		$c18, t1, (_JB_CHERI_C18 * _MIPS_SZCAP/8)($c3)
	csc		$c19, t1, (_JB_CHERI_C19 * _MIPS_SZCAP/8)($c3)
	csc		$c20, t1, (_JB_CHERI_C20 * _MIPS_SZCAP/8)($c3)
	csc		$c21, t1, (_JB_CHERI_C21 * _MIPS_SZCAP/8)($c3)
	csc		$c22, t1, (_JB_CHERI_C22 * _MIPS_SZCAP/8)($c3)
	csc		$c23, t1, (_JB_CHERI_C23 * _MIPS_SZCAP/8)($c3)
	csc		$c24, t1, (_JB_CHERI_C24 * _MIPS_SZCAP/8)($c3)
	cgetdefault	$c4
	csc		$c4, t1, (_JB_CHERI_DDC * _MIPS_SZCAP/8)($c3)

	move		v0, zero
	cjr		$c17
END(setjmp)

#define LONGJMP_FRAME_SIZE	(CALLFRAME_SIZ + _MIPS_SZCAP/8)

NESTED(longjmp, LONGJMP_FRAME_SIZE, ra)
	.protected _C_LABEL(longjmp)
	.mask	0xc0000000, (CALLFRAME_RA - CALLFRAME_SIZ)
	PTR_SUBU	sp, sp, LONGJMP_FRAME_SIZE	# allocate stack frame

	csd		ra, sp, CALLFRAME_RA($c11)
	csd		s8, sp, CALLFRAME_FP($c11)
	csd		a0, sp, CALLFRAME_S0($c11)	# save return value
	csc		$c17, sp, CALLFRAME_C17($c11)
	csc		$c3, sp, CALLFRAME_SIZ($c11)
	move		s8, sp

	cld		v0, zero, (_JB_MAGIC  * SZREG)($c3)
	REG_LI		t0, _JB_MAGIC_SETJMP
	bne		v0, t0, botch			# jump if error
	nop

	# set sigmask
	li		t0, _JB_SIGMASK * SZREG
	csetoffset	$c3, $c3, t0			# set
	cfromptr	$c4, $c3, zero			# oset == NULL
	li		a0, 3				# SIG_SETMASK
	PIC_CALL(__cheriabi_sigprocmask)		# set current signal mask

	move		sp, s8

	clc		$c3, sp, CALLFRAME_SIZ($c11)
	cld		a0, sp, CALLFRAME_S0($c11)	# restore return value

	cld		ra, zero, (_JB_REG_RA * SZREG)($c3)
	/*
	 * From "MIPSpro N32 ABI Handbook", Table 2-1:
	 * Registers s0..s7 are callee-saved.
	 * The sp register is callee-saved.
	 * The fp (or s8) register is callee-saved.
	 * The gp register is callee-saved (for n32/n64).
	 */
	cld		s0, zero, (_JB_REG_S0 * SZREG)($c3)
	cld		s1, zero, (_JB_REG_S1 * SZREG)($c3)
	cld		s2, zero, (_JB_REG_S2 * SZREG)($c3)
	cld		s3, zero, (_JB_REG_S3 * SZREG)($c3)
	cld		s4, zero, (_JB_REG_S4 * SZREG)($c3)
	cld		s5, zero, (_JB_REG_S5 * SZREG)($c3)
	cld		s6, zero, (_JB_REG_S6 * SZREG)($c3)
	cld		s7, zero, (_JB_REG_S7 * SZREG)($c3)
	cld		sp, zero, (_JB_REG_SP * SZREG)($c3)
	cld		s8, zero, (_JB_REG_S8 * SZREG)($c3)
	cld		gp, zero, (_JB_REG_GP * SZREG)($c3)
#ifndef __mips_soft_float
	/*
	 * From "MIPSpro N32 ABI Handbook", Table 2-1:
	 * In N32, FP registers F20, F22, F24, F26, F28, F30 are callee-saved.
	 * In N64, FP registers F23 .. F31 are callee-saved.
	 * In O32, FP registers F20 .. F23 are callee-saved.
	 */
        clw		v0, zero, (_JB_FPREG_FCSR * SZREG)($c3)
        ctc1		v0, $31
	cldc1		$f24, zero, (_JB_FPREG_F24 * SZREG)($c3)
	cldc1		$f25, zero, (_JB_FPREG_F25 * SZREG)($c3)
	cldc1		$f26, zero, (_JB_FPREG_F26 * SZREG)($c3)
	cldc1		$f27, zero, (_JB_FPREG_F27 * SZREG)($c3)
	cldc1		$f28, zero, (_JB_FPREG_F28 * SZREG)($c3)
	cldc1		$f29, zero, (_JB_FPREG_F29 * SZREG)($c3)
	cldc1		$f30, zero, (_JB_FPREG_F30 * SZREG)($c3)
	cldc1		$f31, zero, (_JB_FPREG_F31 * SZREG)($c3)
#endif	/* ! __mips_soft_float */
	/* jmp_buf is 64-bit aligned, so we need to round it up. */
	cgetbase	t0, $c3
	cgetoffset	t1, $c3
	daddu		t1, t0, t1
	daddiu		t0, t1, ((_JB_CHERI_START * SZREG) + _MIPS_SZCAP/8 - SZREG)
	li		t2, _MIPS_CAP_ALIGN_MASK
	and		t2, t2, t0
	dsubu		t1, t2, t1

	clc		$c11, t1, (_JB_CHERI_C11 * _MIPS_SZCAP/8)($c3)
	clc		$c12, t1, (_JB_CHERI_C12 * _MIPS_SZCAP/8)($c3)
	clc		$c13, t1, (_JB_CHERI_C13 * _MIPS_SZCAP/8)($c3)
	clc		$c14, t1, (_JB_CHERI_C14 * _MIPS_SZCAP/8)($c3)
	clc		$c15, t1, (_JB_CHERI_C15 * _MIPS_SZCAP/8)($c3)
	clc		$c16, t1, (_JB_CHERI_C16 * _MIPS_SZCAP/8)($c3)
	clc		$c17, t1, (_JB_CHERI_C17 * _MIPS_SZCAP/8)($c3)
	clc		$c18, t1, (_JB_CHERI_C18 * _MIPS_SZCAP/8)($c3)
	clc		$c19, t1, (_JB_CHERI_C19 * _MIPS_SZCAP/8)($c3)
	clc		$c20, t1, (_JB_CHERI_C20 * _MIPS_SZCAP/8)($c3)
	clc		$c21, t1, (_JB_CHERI_C21 * _MIPS_SZCAP/8)($c3)
	clc		$c22, t1, (_JB_CHERI_C22 * _MIPS_SZCAP/8)($c3)
	clc		$c23, t1, (_JB_CHERI_C23 * _MIPS_SZCAP/8)($c3)
	clc		$c24, t1, (_JB_CHERI_C24 * _MIPS_SZCAP/8)($c3)

	move		v0, a0
	cjr		$c17
	nop

botch:
	/*
	 * We know we aren't returning so we don't care about restoring
	 * our caller's GP.
	 */
	ld		v1, 0(zero)
	PIC_CALL(__cheriabi_longjmperror)
	PIC_TAILCALL(__cheriabi_abort)
END(longjmp)
