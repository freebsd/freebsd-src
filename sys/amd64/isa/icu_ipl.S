/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
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
 * $FreeBSD$
 */

	.data
	ALIGN_DATA

/* interrupt mask enable (all h/w off) */
	.globl	imen
imen:	.long	HWI_MASK

	.text
	SUPERALIGN_TEXT

ENTRY(INTREN)
	movl	4(%esp), %eax
	movl	%eax, %ecx
	notl	%eax
	andl	%eax, imen
	movl	imen, %eax
	testb	%cl, %cl
	je	1f
	outb	%al, $(IO_ICU1 + ICU_IMR_OFFSET)
1:
	testb	%ch, %ch
	je	2f
	shrl	$8, %eax
	outb	%al, $(IO_ICU2 + ICU_IMR_OFFSET)
2:
	ret

ENTRY(INTRDIS)
	movl	4(%esp), %eax
	movl	%eax, %ecx
	orl	%eax, imen
	movl	imen, %eax
	testb	%cl, %cl
	je	1f
	outb	%al, $(IO_ICU1 + ICU_IMR_OFFSET)
1:
	testb	%ch, %ch
	je	2f
	shrl	$8, %eax
	outb	%al, $(IO_ICU2 + ICU_IMR_OFFSET)
2:
	ret
