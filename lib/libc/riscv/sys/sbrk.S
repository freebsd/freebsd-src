/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

	.globl	_C_LABEL(_end)

	.data
	.align	3
	.global	_C_LABEL(curbrk)
	.type	_C_LABEL(curbrk), %object
_C_LABEL(curbrk):
	.quad	_C_LABEL(_end)

	.text
/*
 * void *sbrk(intptr_t incr);
 */
ENTRY(_sbrk)
	WEAK_REFERENCE(_sbrk, sbrk)

	/* Load the address of curbrk */
	la	a3, curbrk

	/* Get the current brk address */
	ld	a2, 0(a3)

	/* Calculate the new value */
	add	a0, a0, a2
	mv	a4, a0

	/* Update for this value, will overwrite a0 and a1 */
	_SYSCALL(break)
	bnez	t0, cerror

	/* Load the old value to return */
	ld	a0, 0(a3)

	/* Store the new curbrk value */
	sd	a4, 0(a3)

	ret
END(_sbrk)
