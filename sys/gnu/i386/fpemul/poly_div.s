	.file	"poly_div.S"
/*
 *  poly_div.S
 *
 * A set of functions to divide 64 bit integers by fixed numbers.
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
 *     $Id: poly_div.s,v 1.3 1994/06/10 07:44:36 rich Exp $
 *
 */

#include <gnu/i386/fpemul/fpu_asm.h>

.text

/*---------------------------------------------------------------------------*/
	.align 2,144
.globl _poly_div2
_poly_div2:
	pushl %ebp
	movl %esp,%ebp

	movl PARAM1,%ecx
	movw (%ecx),%ax

	shrl $1,4(%ecx)
	rcrl $1,(%ecx)

	testw $1,%ax
	je poly_div2_exit

	addl $1,(%ecx)
	adcl $0,4(%ecx)
poly_div2_exit:

	leave
	ret
/*---------------------------------------------------------------------------*/
	.align 2,144
.globl _poly_div4
_poly_div4:
	pushl %ebp
	movl %esp,%ebp

	movl PARAM1,%ecx
	movw (%ecx),%ax

	movl 4(%ecx),%edx
	shll $30,%edx

	shrl $2,4(%ecx)
	shrl $2,(%ecx)

	orl %edx,(%ecx)

	testw $2,%ax
	je poly_div4_exit

	addl $1,(%ecx)
	adcl $0,4(%ecx)
poly_div4_exit:

	leave
	ret
/*---------------------------------------------------------------------------*/
	.align 2,144
.globl _poly_div16
_poly_div16:
	pushl %ebp
	movl %esp,%ebp

	movl PARAM1,%ecx
	movw (%ecx),%ax

	movl 4(%ecx),%edx
	shll $28,%edx

	shrl $4,4(%ecx)
	shrl $4,(%ecx)

	orl %edx,(%ecx)

	testw $8,%ax
	je poly_div16_exit

	addl $1,(%ecx)
	adcl $0,4(%ecx)
poly_div16_exit:

	leave
	ret
/*---------------------------------------------------------------------------*/
