/*-
 * Copyright (c) 1993
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
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS) 
	.text
	.asciz "$OpenBSD$"
#endif /* LIBC_SCCS */

#define DEXP_INF	0x7ff

/*
 * isnan(x)
 *	double x;
 *
 * Return true if x is a NAN.
 */
LEAF(isnan)
	.set	noreorder
	mfc1	v1, $f13		# get MSW of x
	mfc1	t3, $f12		# get LSW of x
	sll	t1, v1, 1		# get x exponent
	srl	t1, t1, 32 - 11
	bne	t1, DEXP_INF, 2f	# is it a finite number?
	sll	t2, v1, 32 - 20		# get x fraction
	bne	t3, zero, 1f		# is it a NAN?
	nop
	beq	t2, zero, 2f		# its infinity
	nop
1:
	j	ra
	li	v0, 1			# x is a NAN
2:
	j	ra
	move	v0, zero		# x is NOT a NAN
END(isnan)

/*
 * isinf(x)
 *	double x;
 *
 * Return true if x is infinity.
 */
LEAF(isinf)
	.set	noreorder
	mfc1	v1, $f13		# get MSW of x
	mfc1	t3, $f12		# get LSW of x
	sll	t1, v1, 1		# get x exponent
	srl	t1, t1, 32 - 11
	bne	t1, DEXP_INF, 1f	# is it a finite number?
	sll	t2, v1, 32 - 20		# get x fraction
	bne	t3, zero, 1f		# is it a NAN?
	nop
	bne	t2, zero, 1f		# is it a NAN?
	nop
	j	ra
	li	v0, 1			# x is infinity
1:
	j	ra
	move	v0, zero		# x is NOT infinity
END(isinf)
