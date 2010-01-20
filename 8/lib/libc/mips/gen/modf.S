/*	$NetBSD: modf.S,v 1.10 2003/08/07 16:42:15 agc Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1995
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

#if defined(LIBC_SCCS) && !defined(lint)
	ASMSTR("from: @(#)modf.s	8.1 (Berkeley) 6/4/93")
	ASMSTR("$NetBSD: modf.S,v 1.10 2003/08/07 16:42:15 agc Exp $")
#endif /* LIBC_SCCS and not lint */

#ifdef __ABICALLS__
	.abicalls
#endif

/*
 * double modf(val, iptr)
 *	double val, *iptr;
 * returns: xxx and n (in *iptr) where val == n.xxx
 */
LEAF(modf)
#ifdef __ABICALLS__
	.set	noreorder
	.cpload	t9
	.set	reorder
#endif
	cfc1	t0, $31			# get the control register
	li.d	$f2, 4503599627370496e0 # f2 <- 2^52

	or	t1, t0, 0x3		# set rounding mode to round to zero
	xor	t1, t1, 0x2		#  (i.e., 01)
	ctc1	t1, $31

	mov.d	$f0, $f12		# f0 <- f12
	abs.d	$f4, $f12		# f4 <- |f12|
	c.olt.d $f4, $f2		# f4 ? < f2 
	bc1f	1f			# leave f0 alone if Nan, infinity 
					# or >=2^52
	c.eq.d	$f12,$f4		# was f12 positive ?
	add.d	$f4,$f2,$f4		# round off to integer
	bc1f	2f			# No -> will have to negate result
	sub.d	$f0,$f4,$f2		# Remove fudge factor
	j	1f			# integer fraction got
2:	
	sub.d	$f0,$f2,$f4		# Remove fudge factor and negate
1:	
	ctc1	t0, $31			# restore old rounding mode
	s.d	$f0, 0(a2)		# save the integer part
	sub.d	$f0, $f12, $f0		# subtract val - integer part
	j	ra
END(modf)
