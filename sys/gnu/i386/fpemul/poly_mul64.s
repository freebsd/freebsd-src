/*
 *  poly_mul64.S
 *
 * Multiply two 64 bit integers.
 *
 * Call from C as:
 *   void mul64(long long *a, long long *b, long long *result)
 *
 *
 * Copyright (C) 1992,1993,1994
 *                       W. Metzenthen, 22 Parker St, Ormond, Vic 3163,
 *                       Australia.  E-mail   billm@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD, FreeBSD and NetBSD operating systems. Any other
 * use is not permitted under this copyright.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must include information specifying
 *    that source code for the emulator is freely available and include
 *    either:
 *      a) an offer to provide the source code for a nominal distribution
 *         fee, or
 *      b) list at least two alternative methods whereby the source
 *         can be obtained, e.g. a publically accessible bulletin board
 *         and an anonymous ftp site from which the software can be
 *         downloaded.
 * 3. All advertising materials specifically mentioning features or use of
 *    this emulator must acknowledge that it was developed by W. Metzenthen.
 * 4. The name of W. Metzenthen may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * W. METZENTHEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * The purpose of this copyright, based upon the Berkeley copyright, is to
 * ensure that the covered software remains freely available to everyone.
 *
 * The software (with necessary differences) is also available, but under
 * the terms of the GNU copyleft, for the Linux operating system and for
 * the djgpp ms-dos extender.
 *
 * W. Metzenthen   June 1994.
 *
 *
 * $FreeBSD$
 *
 */


#include <gnu/i386/fpemul/fpu_asm.h>

.text
ENTRY(mul64)
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %esi
	pushl %ebx

	movl PARAM1,%esi
	movl PARAM2,%ecx
	movl PARAM3,%ebx

	xor %eax,%eax
	movl %eax,-4(%ebp)
	movl %eax,-8(%ebp)

	movl (%esi),%eax
	mull (%ecx)
	movl %eax,-16(%ebp)	/* Not used */
	movl %edx,-12(%ebp)

	movl (%esi),%eax
	mull 4(%ecx)
	addl %eax,-12(%ebp)
	adcl %edx,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 4(%esi),%eax
	mull (%ecx)
	addl %eax,-12(%ebp)
	adcl %edx,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 4(%esi),%eax
	mull 4(%ecx)
	addl %eax,-8(%ebp)
	adcl %edx,-4(%ebp)

	testb $128,-9(%ebp)
	je L_no_round

	addl $1,-8(%ebp)
	adcl $0,-4(%ebp)

L_no_round:
	movl -8(%ebp),%esi
	movl %esi,(%ebx)
	movl -4(%ebp),%esi
	movl %esi,4(%ebx)

	popl %ebx
	popl %esi
	leave
	ret
