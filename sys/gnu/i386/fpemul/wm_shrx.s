	.file	"wm_shrx.S"
/*
 *  wm_shrx.S
 *
 * 64 bit right shift functions
 *
 * Call from C as:
 *   unsigned shrx(void *arg1, unsigned arg2)
 * and
 *   unsigned shrxs(void *arg1, unsigned arg2)
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
 * $FreeBSD: src/sys/gnu/i386/fpemul/wm_shrx.s,v 1.8 1999/08/28 00:42:59 peter Exp $
 *
 */


#include <gnu/i386/fpemul/fpu_asm.h>

.text

/*---------------------------------------------------------------------------+
 |   unsigned shrx(void *arg1, unsigned arg2)                                |
 |                                                                           |
 |   Extended shift right function.                                          |
 |   Fastest for small shifts.                                               |
 |   Shifts the 64 bit quantity pointed to by the first arg (arg1)           |
 |   right by the number of bits specified by the second arg (arg2).         |
 |   Forms a 96 bit quantity from the 64 bit arg and eax:                    |
 |                [  64 bit arg ][ eax ]                                     |
 |            shift right  --------->                                        |
 |   The eax register is initialized to 0 before the shifting.               |
 |   Results returned in the 64 bit arg and eax.                             |
 +---------------------------------------------------------------------------*/

ENTRY(shrx)
	push	%ebp
	movl	%esp,%ebp
	pushl	%esi
	movl	PARAM2,%ecx
	movl	PARAM1,%esi
	cmpl	$32,%ecx	/* shrd only works for 0..31 bits */
	jnc	L_more_than_31

/* less than 32 bits */
	pushl	%ebx
	movl	(%esi),%ebx	/* lsl */
	movl	4(%esi),%edx	/* msl */
	xorl	%eax,%eax	/* extension */
	shrd	%cl,%ebx,%eax
	shrd	%cl,%edx,%ebx
	shr	%cl,%edx
	movl	%ebx,(%esi)
	movl	%edx,4(%esi)
	popl	%ebx
	popl	%esi
	leave
	ret

L_more_than_31:
	cmpl	$64,%ecx
	jnc	L_more_than_63

	subb	$32,%cl
	movl	(%esi),%eax	/* lsl */
	movl	4(%esi),%edx	/* msl */
	shrd	%cl,%edx,%eax
	shr	%cl,%edx
	movl	%edx,(%esi)
	movl	$0,4(%esi)
	popl	%esi
	leave
	ret

L_more_than_63:
	cmpl	$96,%ecx
	jnc	L_more_than_95

	subb	$64,%cl
	movl	4(%esi),%eax	/* msl */
	shr	%cl,%eax
	xorl	%edx,%edx
	movl	%edx,(%esi)
	movl	%edx,4(%esi)
	popl	%esi
	leave
	ret

L_more_than_95:
	xorl	%eax,%eax
	movl	%eax,(%esi)
	movl	%eax,4(%esi)
	popl	%esi
	leave
	ret


/*---------------------------------------------------------------------------+
 |   unsigned shrxs(void *arg1, unsigned arg2)                               |
 |                                                                           |
 |   Extended shift right function (optimized for small floating point       |
 |   integers).                                                              |
 |   Shifts the 64 bit quantity pointed to by the first arg (arg1)           |
 |   right by the number of bits specified by the second arg (arg2).         |
 |   Forms a 96 bit quantity from the 64 bit arg and eax:                    |
 |                [  64 bit arg ][ eax ]                                     |
 |            shift right  --------->                                        |
 |   The eax register is initialized to 0 before the shifting.               |
 |   The lower 8 bits of eax are lost and replaced by a flag which is        |
 |   set (to 0x01) if any bit, apart from the first one, is set in the       |
 |   part which has been shifted out of the arg.                             |
 |   Results returned in the 64 bit arg and eax.                             |
 +---------------------------------------------------------------------------*/
	.globl	_shrxs
_shrxs:
	push	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%ebx
	movl	PARAM2,%ecx
	movl	PARAM1,%esi
	cmpl	$64,%ecx	/* shrd only works for 0..31 bits */
	jnc	Ls_more_than_63

	cmpl	$32,%ecx	/* shrd only works for 0..31 bits */
	jc	Ls_less_than_32

/* We got here without jumps by assuming that the most common requirement
   is for small integers */
/* Shift by [32..63] bits */
	subb	$32,%cl
	movl	(%esi),%eax	/* lsl */
	movl	4(%esi),%edx	/* msl */
	xorl	%ebx,%ebx
	shrd	%cl,%eax,%ebx
	shrd	%cl,%edx,%eax
	shr	%cl,%edx
	orl	%ebx,%ebx		/* test these 32 bits */
	setne	%bl
	test	$0x7fffffff,%eax	/* and 31 bits here */
	setne	%bh
	orw	%bx,%bx			/* Any of the 63 bit set ? */
	setne	%al
	movl	%edx,(%esi)
	movl	$0,4(%esi)
	popl	%ebx
	popl	%esi
	leave
	ret

/* Shift by [0..31] bits */
Ls_less_than_32:
	movl	(%esi),%ebx	/* lsl */
	movl	4(%esi),%edx	/* msl */
	xorl	%eax,%eax	/* extension */
	shrd	%cl,%ebx,%eax
	shrd	%cl,%edx,%ebx
	shr	%cl,%edx
	test	$0x7fffffff,%eax	/* only need to look at eax here */
	setne	%al
	movl	%ebx,(%esi)
	movl	%edx,4(%esi)
	popl	%ebx
	popl	%esi
	leave
	ret

/* Shift by [64..95] bits */
Ls_more_than_63:
	cmpl	$96,%ecx
	jnc	Ls_more_than_95

	subb	$64,%cl
	movl	(%esi),%ebx	/* lsl */
	movl	4(%esi),%eax	/* msl */
	xorl	%edx,%edx	/* extension */
	shrd	%cl,%ebx,%edx
	shrd	%cl,%eax,%ebx
	shr	%cl,%eax
	orl	%ebx,%edx
	setne	%bl
	test	$0x7fffffff,%eax	/* only need to look at eax here */
	setne	%bh
	orw	%bx,%bx
	setne	%al
	xorl	%edx,%edx
	movl	%edx,(%esi)	/* set to zero */
	movl	%edx,4(%esi)	/* set to zero */
	popl	%ebx
	popl	%esi
	leave
	ret

Ls_more_than_95:
/* Shift by [96..inf) bits */
	xorl	%eax,%eax
	movl	(%esi),%ebx
	orl	4(%esi),%ebx
	setne	%al
	xorl	%ebx,%ebx
	movl	%ebx,(%esi)
	movl	%ebx,4(%esi)
	popl	%ebx
	popl	%esi
	leave
	ret
