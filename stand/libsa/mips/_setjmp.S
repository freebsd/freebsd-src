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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 * $FreeBSD$
 */

#include <machine/asm.h>

#if 0
#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: _setjmp.S,v 1.6 1996/09/23 21:27:53 imp Exp $"
#endif /* LIBC_SCCS */
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
	.set	noreorder
	REG_LI	v0, _JB_MAGIC__SETJMP		# sigcontext magic number
	REG_S	v0, (_JB_MAGIC * SZREG)(a0)	#   saved in sc_regs[0]
	REG_S	ra, (_JB_REG_RA * SZREG)(a0)	# sc_pc = return address
	REG_S	s0, (_JB_REG_S0 * SZREG)(a0)
	REG_S	s1, (_JB_REG_S1 * SZREG)(a0)
	REG_S	s2, (_JB_REG_S2 * SZREG)(a0)
	REG_S	s3, (_JB_REG_S3 * SZREG)(a0)
	REG_S	s4, (_JB_REG_S4 * SZREG)(a0)
	REG_S	s5, (_JB_REG_S5 * SZREG)(a0)
	REG_S	s6, (_JB_REG_S6 * SZREG)(a0)
	REG_S	s7, (_JB_REG_S7 * SZREG)(a0)
	REG_S	sp, (_JB_REG_SP * SZREG)(a0)
	REG_S	s8, (_JB_REG_S8 * SZREG)(a0)
	j	ra
	move	v0, zero
END(_setjmp)

LEAF(_longjmp)
#ifdef ABICALLS 
	subu	sp, sp, 32
	.cprestore 16
#endif  
	.set    noreorder
	REG_L	v0, (_JB_MAGIC * SZREG)(a0)	# get magic number
	REG_L	ra, (_JB_REG_RA * SZREG)(a0)
	bne	v0, _JB_MAGIC__SETJMP, botch	# jump if error

	addu	sp, sp, 32			# does not matter, sanity
	REG_L	s0, (_JB_REG_S0 * SZREG)(a0)
	REG_L	s1, (_JB_REG_S1 * SZREG)(a0)
	REG_L	s2, (_JB_REG_S2 * SZREG)(a0)
	REG_L	s3, (_JB_REG_S3 * SZREG)(a0)
	REG_L	s4, (_JB_REG_S4 * SZREG)(a0)
	REG_L	s5, (_JB_REG_S5 * SZREG)(a0)
	REG_L	s6, (_JB_REG_S6 * SZREG)(a0)
	REG_L	s7, (_JB_REG_S7 * SZREG)(a0)
	REG_L	sp, (_JB_REG_SP * SZREG)(a0)
	REG_L	s8, (_JB_REG_S8 * SZREG)(a0)

	j	ra
	move	v0, a1
botch:
	jal	_C_LABEL(longjmperror)
	nop
	jal	_C_LABEL(abort)
	nop
END(_longjmp)
