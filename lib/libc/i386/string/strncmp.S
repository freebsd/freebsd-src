/*
 * Copyright (c) 1993,94 Winning Strategies, Inc.
 * All rights reserved.
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
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: strncmp.S,v 1.3 1994/03/31 14:11:02 davidg Exp $
 */

#if defined(LIBC_RCS) && !defined(lint)
        .asciz "$Id: strncmp.S,v 1.3 1994/03/31 14:11:02 davidg Exp $"
#endif /* LIBC_RCS and not lint */

#include "DEFS.h"

/*
 * strncmp(s1, s2, n)
 *	return an integer greater than, equal to, or less than 0,
 *	according as the first n characters of string s1 is greater
 *	than, equal to, or less than the string s2.
 *
 * %eax - pointer to s1
 * %ecx - pointer to s2
 * %edx - length
 *
 * Written by:
 *	J.T. Conklin (jtc@wimsey.com), Winning Strategies, Inc.
 */

/*
 * I've unrolled the loop eight times: large enough to make a
 * significant difference, and small enough not to totally trash the
 * cache.
 */

ENTRY(strncmp)
	pushl	%ebx
	movl	8(%esp),%eax
	movl	12(%esp),%ecx
	movl	16(%esp),%edx
	testl	%edx,%edx
	jmp	L2			/* Jump into the loop! */

	.align 2,0x90
L1:	incl	%eax
	incl	%ecx
	decl	%edx
L2:	jz	L4			/* strings are equal */
	movb	(%eax),%bl
	testb	%bl,%bl
	jz	L3
	cmpb	%bl,(%ecx)
	jne	L3

	incl	%eax
	incl	%ecx
	decl	%edx
	jz	L4
	movb	(%eax),%bl
	testb	%bl,%bl
	jz	L3
	cmpb	%bl,(%ecx)
	jne	L3

	incl	%eax
	incl	%ecx
	decl	%edx
	jz	L4
	movb	(%eax),%bl
	testb	%bl,%bl
	jz	L3
	cmpb	%bl,(%ecx)
	jne	L3

	incl	%eax
	incl	%ecx
	decl	%edx
	jz	L4
	movb	(%eax),%bl
	testb	%bl,%bl
	jz	L3
	cmpb	%bl,(%ecx)
	jne	L3

	incl	%eax
	incl	%ecx
	decl	%edx
	jz	L4
	movb	(%eax),%bl
	testb	%bl,%bl
	jz	L3
	cmpb	%bl,(%ecx)
	jne	L3

	incl	%eax
	incl	%ecx
	decl	%edx
	jz	L4
	movb	(%eax),%bl
	testb	%bl,%bl
	jz	L3
	cmpb	%bl,(%ecx)
	jne	L3

	incl	%eax
	incl	%ecx
	decl	%edx
	jz	L4
	movb	(%eax),%bl
	testb	%bl,%bl
	jz	L3
	cmpb	%bl,(%ecx)
	jne	L3

	incl	%eax
	incl	%ecx
	decl	%edx
	jz	L4
	movb	(%eax),%bl
	testb	%bl,%bl
	jz	L3
	cmpb	%bl,(%ecx)
	je	L1

	.align 2,0x90
L3:	movzbl	(%eax),%eax		/* unsigned comparision */
	movzbl	(%ecx),%ecx
	subl	%ecx,%eax
	popl	%ebx
	ret
	.align 2,0x90
L4:	xorl	%eax,%eax
	popl	%ebx
	ret
