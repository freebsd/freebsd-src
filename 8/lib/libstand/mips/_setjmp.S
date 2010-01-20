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

#include <machine/regnum.h>
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
	li	v0, 0xACEDBADE			# sigcontext magic number
	sw	ra, (2 * 4)(a0)			# sc_pc = return address
	sw	v0, (3 * 4)(a0)			#   saved in sc_regs[0]
	sw	s0, ((S0 + 3) * 4)(a0)
	sw	s1, ((S1 + 3) * 4)(a0)
	sw	s2, ((S2 + 3) * 4)(a0)
	sw	s3, ((S3 + 3) * 4)(a0)
	sw	s4, ((S4 + 3) * 4)(a0)
	sw	s5, ((S5 + 3) * 4)(a0)
	sw	s6, ((S6 + 3) * 4)(a0)
	sw	s7, ((S7 + 3) * 4)(a0)
	sw	sp, ((SP + 3) * 4)(a0)
	sw	s8, ((S8 + 3) * 4)(a0)
	cfc1	v0, $31				# too bad cant check if FP used
	swc1	$f20, ((20 + 38) * 4)(a0)
	swc1	$f21, ((21 + 38) * 4)(a0)
	swc1	$f22, ((22 + 38) * 4)(a0)
	swc1	$f23, ((23 + 38) * 4)(a0)
	swc1	$f24, ((24 + 38) * 4)(a0)
	swc1	$f25, ((25 + 38) * 4)(a0)
	swc1	$f26, ((26 + 38) * 4)(a0)
	swc1	$f27, ((27 + 38) * 4)(a0)
	swc1	$f28, ((28 + 38) * 4)(a0)
	swc1	$f29, ((29 + 38) * 4)(a0)
	swc1	$f30, ((30 + 38) * 4)(a0)
	swc1	$f31, ((31 + 38) * 4)(a0)
	sw	v0, ((32 + 38) * 4)(a0)
	j	ra
	move	v0, zero
END(_setjmp)

LEAF(_longjmp)
#ifdef ABICALLS 
	subu	sp, sp, 32
	.cprestore 16
#endif  
	.set    noreorder
	lw	v0, (3 * 4)(a0)			# get magic number
	lw	ra, (2 * 4)(a0)
	bne	v0, 0xACEDBADE, botch		# jump if error

	addu	sp, sp, 32			# does not matter, sanity
	lw	s0, ((S0 + 3) * 4)(a0)
	lw	s1, ((S1 + 3) * 4)(a0)
	lw	s2, ((S2 + 3) * 4)(a0)
	lw	s3, ((S3 + 3) * 4)(a0)
	lw	s4, ((S4 + 3) * 4)(a0)
	lw	s5, ((S5 + 3) * 4)(a0)
	lw	s6, ((S6 + 3) * 4)(a0)
	lw	s7, ((S7 + 3) * 4)(a0)
	lw	v0, ((32 + 38) * 4)(a0)		# get fpu status
	lw	sp, ((SP + 3) * 4)(a0)
	lw	s8, ((S8 + 3) * 4)(a0)
/* Octeon does not have an FPU */
#if !defined(_MIPS_ARCH_OCTEON)
	ctc1	v0, $31
	lwc1	$f20, ((20 + 38) * 4)(a0)
	lwc1	$f21, ((21 + 38) * 4)(a0)
	lwc1	$f22, ((22 + 38) * 4)(a0)
	lwc1	$f23, ((23 + 38) * 4)(a0)
	lwc1	$f24, ((24 + 38) * 4)(a0)
	lwc1	$f25, ((25 + 38) * 4)(a0)
	lwc1	$f26, ((26 + 38) * 4)(a0)
	lwc1	$f27, ((27 + 38) * 4)(a0)
	lwc1	$f28, ((28 + 38) * 4)(a0)
	lwc1	$f29, ((29 + 38) * 4)(a0)
	lwc1	$f30, ((30 + 38) * 4)(a0)
	lwc1	$f31, ((31 + 38) * 4)(a0)
#endif /*  _MIPS_ARCH_OCTEON */

	j	ra
	move	v0, a1
botch:
	jal	_C_LABEL(longjmperror)
	nop
	jal	_C_LABEL(abort)
	nop
END(_longjmp)
