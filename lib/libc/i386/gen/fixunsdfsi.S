/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)fixunsdfsi.s	5.1	12/17/90
 *	$Id: fixunsdfsi.S,v 1.2 1994/12/27 13:37:38 bde Exp $
 */

#if defined(LIBC_RCS) && !defined(lint)
	.text
	.asciz "$Id: fixunsdfsi.S,v 1.2 1994/12/27 13:37:38 bde Exp $"
#endif /* LIBC_RCS and not lint */

#include "DEFS.h"

ENTRY(__fixunsdfsi)
	fldl	4(%esp)
	fnstcw	4(%esp)
	movl	4(%esp),%eax
	orb	$(3 << 2),%ah	# change rounding mode bitfield to 3 (chop)
	movl	%eax,8(%esp)
	fldcw	8(%esp)
	subl	$8,%esp
	fistpq	(%esp)		# convert to 64 bit integer
	popl	%eax
	popl	%edx		# discard top 32 bits
	fldcw	4(%esp)
	ret

/*
 * XXX - we are sloppy about overflow, the same as gcc-2.  Values too big
 * for a 64-bit (signed) integer cause an overflow trap or are silently
 * converted to 0 if the overflow trap is masked.  The remaining values
 * too big for a 32-bit (unsigned) integer are silently truncated to 32
 * bits.  Negative values are converted like negative ints (no overflow)
 * if they fit in 32 bits.
 */
