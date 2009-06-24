/*	$NetBSD: _setjmp.S,v 1.20 2005/10/07 17:16:40 tsutsui Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(lint)
	ASMSTR("from: @(#)_setjmp.s	8.1 (Berkeley) 6/4/93")
	ASMSTR("$NetBSD: _setjmp.S,v 1.20 2005/10/07 17:16:40 tsutsui Exp $")
#endif /* LIBC_SCCS and not lint */

#ifdef __ABICALLS__
	.abicalls
#endif

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


LEAF(_setjmp)
#ifdef __ABICALLS__ 
	.set	noreorder
	.cpload t9
	subu	sp, sp, CALLFRAME_SIZ	# allocate stack frame
	.cprestore 16
#endif
	li	v0, _JB_MAGIC__SETJMP
	sw	v0, (_JB_MAGIC  * SZREG)(a0)
	sw	ra, (_JB_REG_RA * SZREG)(a0)
	sw	s0, (_JB_REG_S0 * SZREG)(a0)
	sw	s1, (_JB_REG_S1 * SZREG)(a0)
	sw	s2, (_JB_REG_S2 * SZREG)(a0)
	sw	s3, (_JB_REG_S3 * SZREG)(a0)
	sw	s4, (_JB_REG_S4 * SZREG)(a0)
	sw	s5, (_JB_REG_S5 * SZREG)(a0)
	sw	s6, (_JB_REG_S6 * SZREG)(a0)
	sw	s7, (_JB_REG_S7 * SZREG)(a0)
	sw	s8, (_JB_REG_S8 * SZREG)(a0)
#ifdef __ABICALLS__
	addu	sp, sp, CALLFRAME_SIZ	# un-allocate the stack frame
#endif	
	sw	sp, (_JB_REG_SP * SZREG)(a0)
	j	ra
	move	v0, zero
END(_setjmp)

LEAF(_longjmp)
#ifdef __ABICALLS__ 
	.set	noreorder
	.cpload t9
	subu	sp, sp, CALLFRAME_SIZ	# allocate stack frame
	.cprestore 16
#endif  
	lw	v0, (_JB_MAGIC  * SZREG)(a0)
	lw	ra, (_JB_REG_RA * SZREG)(a0)
	li	t0, _JB_MAGIC__SETJMP
	bne	v0, t0, botch		# jump if error
	lw	s0, (_JB_REG_S0 * SZREG)(a0)
	lw	s1, (_JB_REG_S1 * SZREG)(a0)
	lw	s2, (_JB_REG_S2 * SZREG)(a0)
	lw	s3, (_JB_REG_S3 * SZREG)(a0)
	lw	s4, (_JB_REG_S4 * SZREG)(a0)
	lw	s5, (_JB_REG_S5 * SZREG)(a0)
	lw	s6, (_JB_REG_S6 * SZREG)(a0)
	lw	s7, (_JB_REG_S7 * SZREG)(a0)
	lw	sp, (_JB_REG_SP * SZREG)(a0)
	lw	s8, (_JB_REG_S8 * SZREG)(a0)

	j	ra
	move	v0, a1

botch:
	jal	_C_LABEL(longjmperror)
	nop
	jal	_C_LABEL(abort)
	nop
END(_longjmp)
