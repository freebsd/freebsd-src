/*	$NetBSD: biosgetrtc.S,v 1.2 1997/06/13 13:42:27 drochner Exp $	*/

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <machine/asm.h>

#define	addr32	.byte 0x67
#define	data32	.byte 0x66

ENTRY(biosgetrtc)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	xorl	%ebx, %ebx

	call	CNAME(prot_to_real)	# enter real mode

	movb	$2, %ah
	int	$0x1a
	jnc	ok
	data32
	movl	$-1, %ebx

ok:
	data32
	call	CNAME(real_to_prot) # back to protected mode

	movl	8(%ebp), %eax
	movl	$0, (%eax)
	movb	%ch, (%eax)
	movb	%cl, 1(%eax)
	movb	%dh, 2(%eax)

	movl	%ebx, %eax

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret
