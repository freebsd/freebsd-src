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
 *
 *	from: @(#)setjmp.s	5.1 (Berkeley) 4/23/90"
 *	$Id: sigsetjmp.s,v 1.2 1993/10/20 17:37:41 jtc Exp $
 */

#if defined(LIBC_SCCS) && !defined(lint)
	.text
	.asciz "$Id: sigsetjmp.s,v 1.2 1993/10/20 17:37:41 jtc Exp $"
#endif /* LIBC_SCCS and not lint */

#include "DEFS.h"

ENTRY(sigsetjmp)
	movl	8(%esp),%eax
	movl	4(%esp),%ecx 
	movl	%eax,24(%ecx)
	testl	%eax,%eax
	jz	1f
	pushl	$0
	call	_sigblock
	addl	$4,%esp
	movl	4(%esp),%ecx 
	movl	%eax,28(%ecx)
1:	movl	0(%esp),%edx
	movl	%edx, 0(%ecx)
	movl	%ebx, 4(%ecx)
	movl	%esp, 8(%ecx)
	movl	%ebp,12(%ecx)
	movl	%esi,16(%ecx)
	movl	%edi,20(%ecx)
	xorl	%eax,%eax
	ret

ENTRY(siglongjmp)
	movl	4(%esp),%edx
	cmpl	$0,24(%edx)
	jz	1f
	pushl	28(%edx)
	call	_sigsetmask
	addl	$4,%esp
1:	movl	4(%esp),%edx
	movl	8(%esp),%eax
	movl	0(%edx),%ecx
	movl	4(%edx),%ebx
	movl	8(%edx),%esp
	movl	12(%edx),%ebp
	movl	16(%edx),%esi
	movl	20(%edx),%edi
	testl	%eax,%eax
	jnz	2f
	incl	%eax
2:	movl	%ecx,0(%esp)
	ret
