/*	$NetBSD: sbrk.S,v 1.16 2005/04/22 06:58:01 simonb Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");
#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
	ASMSTR("from: @(#)sbrk.s	8.1 (Berkeley) 6/4/93")
	ASMSTR("$NetBSD: sbrk.S,v 1.16 2005/04/22 06:58:01 simonb Exp $")
#endif /* LIBC_SCCS and not lint */

	.globl	_C_LABEL(__curbrk)
	.globl	_C_LABEL(_end)

	.data
_C_LABEL(__curbrk):
	.word	_C_LABEL(_end)
	.text

LEAF(__sys_sbrk)
	WEAK_ALIAS(sbrk, __sys_sbrk)
	WEAK_ALIAS(_sbrk, __sys_sbrk)
#ifdef __ABICALLS__
	.set	noreorder
	.cpload	t9
	.set	reorder
#endif
	addu	sp, sp, -16
	sw	s0, 0(sp)		# Preserve s0 value in stack
					# it should be the same on return
					# We can't use v1 as temporary
					# register since syscall uses it
					# to return 64-bit values
	lw	s0, _C_LABEL(__curbrk)
	li	v0, SYS_break
	addu	a0, a0, s0		# compute current break

	syscall

	bne	a3, zero, 1f
	nop
	move	v0, s0			# return old val of curbrk from above
	lw	s0, 0(sp)
	addu	sp, sp, 16
	sw	a0, _C_LABEL(__curbrk)	# save current val of curbrk from above
	j	ra

1:
	lw	s0, 0(sp)
	addu	sp, sp, 16
	la	t9, _C_LABEL(__cerror)
	jr	t9
END(__sys_sbrk)
