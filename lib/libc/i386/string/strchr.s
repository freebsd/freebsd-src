/*
 * Copyright (c) 1993 Winning Strategies, Inc.
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
 *    derived from this software withough specific prior written permission
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
 *	$Id: strchr.s,v 1.1 1993/08/16 18:40:42 jtc Exp $
 */

#if defined(LIBC_RCS) && !defined(lint)
        .asciz "$Id: strchr.s,v 1.1 1993/08/16 18:40:42 jtc Exp $"
#endif /* LIBC_RCS and not lint */

#include "DEFS.h"

/*
 * strchr(s, c)
 *	return a pointer to the first occurance of the character c in
 *	string s, or NULL if c does not occur in the string.
 *
 * %edx - pointer iterating through string
 * %eax - pointer to first occurance of 'c'
 * %cl  - character we're comparing against
 * %bl  - character at %edx
 *
 * Written by:
 *	J.T. Conklin (jtc@wimsey.com), Winning Strategies, Inc.
 */

ENTRY(strchr)
	pushl	%ebx
	movl	8(%esp),%eax
	movb	12(%esp),%cl
	.align 2,0x90
L1:
	movb	(%eax),%bl
	cmpb	%bl,%cl			/* found char??? */
	je 	L2
	incl	%eax
	testb	%bl,%bl			/* null terminator??? */
	jne	L1
	xorl	%eax,%eax
L2:
	popl	%ebx
	ret
