/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sean Eric Fagan.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	from: @(#)modf.s	5.5 (Berkeley) 3/18/91
 */

#include <machine/asm.h>
#if defined(LIBC_SCCS)
	RCSID("$NetBSD: modf.S,v 1.5 1997/07/16 14:37:18 christos Exp $")
#endif
__FBSDID("$FreeBSD$");

/*
 * modf(value, iptr): return fractional part of value, and stores the
 * integral part into iptr (a pointer to double).
 *
 * Written by Sean Eric Fagan (sef@kithrup.COM)
 * Sun Mar 11 20:27:30 PST 1990
 */

/* With CHOP mode on, frndint behaves as TRUNC does.  Useful. */
ENTRY(modf)

	/*
	 * Set chop mode.
	 */
	fnstcw	-12(%rsp)
	movw	-12(%rsp),%dx
	orw	$3072,%dx
	movw	%dx,-16(%rsp)
	fldcw	-16(%rsp)

	/*
	 * Get integral part.
	 */
	movsd	%xmm0,-24(%rsp)
	fldl	-24(%rsp)
	frndint
	fstpl	-8(%rsp)

	/*
	 * Restore control word.
	 */
	fldcw	-12(%rsp)

	/*
	 * Store integral part.
	 */
	movsd	-8(%rsp),%xmm0
	movsd	%xmm0,(%rdi)

	/*
	 * Get fractional part and return it.
	 */
	fldl	-24(%rsp)
	fsubl	-8(%rsp)
	fstpl	-8(%rsp)
	movsd   -8(%rsp),%xmm0

	ret
END(modf)
