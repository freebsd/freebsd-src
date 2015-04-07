/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include "SYS.h"

	.data
	.align	3
	.globl	_C_LABEL(minbrk)
	.type	_C_LABEL(minbrk),#object
_C_LABEL(minbrk):
	.quad	_C_LABEL(_end)

	.text
/*
 * int brk(const void *addr);
 */
ENTRY(_brk)
	WEAK_REFERENCE(_brk, brk)

	/* Load the address of minbrk */
#ifdef __PIC__
	adrp	x2, :got:minbrk
	ldr	x3, [x2, #:got_lo12:minbrk]
#else
	ldr	x3, .Lminbrk
#endif

	/* Get the minimum allowable brk address */
	ldr	x2, [x3]

	/* Validate the address */
	cmp	x0, x2
	b.ge	1f
	/* Invalid, set it to the minimum */
	mov	x0, x2

	/* Backup the new address */
1:	mov	x4, x0

	/* Update for this value, will overwrite x0 and x1 */
	_SYSCALL(break)
	b.cs	cerror

#ifdef __PIC__
	adrp	x2, :got:curbrk
	ldr	x3, [x2, #:got_lo12:curbrk]
#else
	ldr	x3, .Lcurbrk
#endif

	/* Store the new curbrk value */
	str	x4, [x3]

	/* Return success */
	mov	x0, #0
	ret

#ifndef __PIC__
.Lcurbrk:
	.quad	_C_LABEL(curbrk)
.Lminbrk:
	.quad	_C_LABEL(minbrk)
#endif
END(_brk)
