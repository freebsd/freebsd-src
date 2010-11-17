/*	$NetBSD: _setjmp.S,v 1.20.34.5 2010/02/03 23:46:47 matt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)_setjmp.s	8.1 (Berkeley) 6/4/93")
#else
	RCSID("$NetBSD: _setjmp.S,v 1.20.34.5 2010/02/03 23:46:47 matt Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	_setjmp(a)
 * by restoring registers from the stack,
 * The previous signal state is NOT restored.
 */

	.set	noreorder

LEAF(_setjmp)
	REG_PROLOGUE
	REG_LI	v0, _JB_MAGIC__SETJMP
	REG_S	v0, (_JB_MAGIC  * SZREG)(a0)
	REG_S	ra, (_JB_REG_RA * SZREG)(a0)
	REG_S	s0, (_JB_REG_S0 * SZREG)(a0)
	REG_S	s1, (_JB_REG_S1 * SZREG)(a0)
	REG_S	s2, (_JB_REG_S2 * SZREG)(a0)
	REG_S	s3, (_JB_REG_S3 * SZREG)(a0)
	REG_S	s4, (_JB_REG_S4 * SZREG)(a0)
	REG_S	s5, (_JB_REG_S5 * SZREG)(a0)
	REG_S	s6, (_JB_REG_S6 * SZREG)(a0)
	REG_S	s7, (_JB_REG_S7 * SZREG)(a0)
	REG_S	s8, (_JB_REG_S8 * SZREG)(a0)
#if defined(__mips_n32) || defined(__mips_n64)
	REG_S	gp, (_JB_REG_GP * SZREG)(a0)	# newabi gp is callee-saved
#endif
	REG_S	sp, (_JB_REG_SP * SZREG)(a0)
	REG_EPILOGUE

	j	ra
	move	v0, zero
END(_setjmp)

LEAF(_longjmp)
	PIC_PROLOGUE(_longjmp)
	PTR_SUBU	sp, sp, CALLFRAME_SIZ
	SAVE_GP(CALLFRAME_GP)

	REG_PROLOGUE
	REG_L		v0, (_JB_MAGIC  * SZREG)(a0)	# get magic number
	REG_L		ra, (_JB_REG_RA * SZREG)(a0)
	REG_LI		t0, _JB_MAGIC__SETJMP
	bne		v0, t0, botch		# jump if error
	PTR_ADDU	sp, sp, CALLFRAME_SIZ	# does not matter, sanity
	REG_L		s0, (_JB_REG_S0 * SZREG)(a0)
	REG_L		s1, (_JB_REG_S1 * SZREG)(a0)
	REG_L		s2, (_JB_REG_S2 * SZREG)(a0)
	REG_L		s3, (_JB_REG_S3 * SZREG)(a0)
	REG_L		s4, (_JB_REG_S4 * SZREG)(a0)
	REG_L		s5, (_JB_REG_S5 * SZREG)(a0)
	REG_L		s6, (_JB_REG_S6 * SZREG)(a0)
	REG_L		s7, (_JB_REG_S7 * SZREG)(a0)
#if defined(__mips_n32) || defined(__mips_n64)
	REG_L		gp, (_JB_REG_GP * SZREG)(a0)
#endif
	REG_L		sp, (_JB_REG_SP * SZREG)(a0)
	REG_L		s8, (_JB_REG_S8 * SZREG)(a0)

	REG_EPILOGUE
	move	v0, a1			# get return value in 1st arg
	j	ra
	nop

botch:
	/*
	 * We know we aren't returning so we don't care about restoring
	 * our caller's GP.
	 */
	PTR_LA	t9, _C_LABEL(longjmperror)
	jalr	t9
	nop

	PIC_TAILCALL(abort)
END(_longjmp)
