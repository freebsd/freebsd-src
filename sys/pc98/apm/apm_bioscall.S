/*-
 * Copyright (c) 1997 Jonathan Lemon
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pc98/apm/apm_bioscall.S,v 1.1.22.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Functions for calling x86 BIOS functions from the BSD kernel
 */
	
#include <machine/asmacros.h>

	.text

/*
 * bios32_apm98(regs, offset, segment)
 *	struct bios_regs *regs;
 *	u_int offset;
 * 	u_short segment;
 */
ENTRY(bios32_apm98)
	pushl	%ebp
	movl	16(%esp),%ebp
	mov	%bp,bioscall_vector+4
	movl	12(%esp),%ebp
	movl	%ebp,bioscall_vector
	movl	8(%esp),%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	0(%ebp),%eax
	movl	4(%ebp),%ebx
	movl	8(%ebp),%ecx
	movl	12(%ebp),%edx
	movl	16(%ebp),%esi
	movl	20(%ebp),%edi
	pushl	%ebp
	pushfl
	cli
	lcall	*bioscall_vector
	movl	%eax,%edi
	movl	%edx,%esi
	lahf
	movl	apm_necsmm_addr,%edx
	andl	%edx,%edx
	jz	9f
	inb	%dx,%al
	andl	apm_necsmm_mask,%eax
	outb	%al,%dx
9:	
	sti
	popl	%ebp
	movl	%edi,0(%ebp)
	movl	%ebx,4(%ebp)
	movl	%ecx,8(%ebp)
	movl	%esi,12(%ebp)
	movl	$0,16(%ebp)	/* esi is cannot get */
	movl	$0,20(%ebp)	/* edi is cannot get */
	movl	%edi,%eax
	andl	$0x100,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
