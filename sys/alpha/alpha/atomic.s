/*-
 * Copyright (c) 1998 Doug Rabson
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
 * $FreeBSD$
 */

#include <machine/asm.h>

	.text

LEAF(atomic_set_8, 2)
	bic	a0, 7, t1
0:	ldq_l	t2, 0(t1)
	extbl	t2, a0, t0
	bis	t0, a1, t0
	insbl	t0, a0, t0
	mskbl	t2, a0, t2
	or	t2, t0, t0
	stq_c	t0, 0(t1)
	beq	t0, 1f
	mb
	RET
1:	br	0b
	END(atomic_set_8)

LEAF(atomic_clear_8, 2)
	bic	a0, 7, t1
0:	ldq_l	t2, 0(t1)
	extbl	t2, a0, t0
	bic	t0, a1, t0
	insbl	t0, a0, t0
	mskbl	t2, a0, t2
	or	t2, t0, t0
	stq_c	t0, 0(t1)
	beq	t0, 1f
	mb
	RET
1:	br	0b
	END(atomic_clear_8)
	
LEAF(atomic_add_8, 2)
	bic	a0, 7, t1
0:	ldq_l	t2, 0(t1)
	extbl	t2, a0, t0
	addl	t0, a1, t0
	insbl	t0, a0, t0
	mskbl	t2, a0, t2
	or	t2, t0, t0
	stq_c	t0, 0(t1)
	beq	t0, 1f
	mb
	RET
1:	br	0b
	END(atomic_add_8)

LEAF(atomic_subtract_8, 2)
	bic	a0, 7, t1
0:	ldq_l	t2, 0(t1)
	extbl	t2, a0, t0
	subl	t0, a1, t0
	insbl	t0, a0, t0
	mskbl	t2, a0, t2
	or	t2, t0, t0
	stq_c	t0, 0(t1)
	beq	t0, 1f
	mb
	RET
1:	br	0b
	END(atomic_subtract_8)
	
LEAF(atomic_set_16, 2)
	bic	a0, 7, t1
0:	ldq_l	t2, 0(t1)
	extwl	t2, a0, t0
	bis	t0, a1, t0
	inswl	t0, a0, t0
	mskwl	t2, a0, t2
	or	t2, t0, t0
	stq_c	t0, 0(t1)
	beq	t0, 1f
	mb
	RET
1:	br	0b
	END(atomic_set_16)

LEAF(atomic_clear_16, 2)
	bic	a0, 7, t1
0:	ldq_l	t2, 0(t1)
	extwl	t2, a0, t0
	bic	t0, a1, t0
	inswl	t0, a0, t0
	mskwl	t2, a0, t2
	or	t2, t0, t0
	stq_c	t0, 0(t1)
	beq	t0, 1f
	mb
	RET
1:	br	0b
	END(atomic_clear_16)
	
LEAF(atomic_add_16, 2)
	bic	a0, 7, t1
0:	ldq_l	t2, 0(t1)
	extwl	t2, a0, t0
	addl	t0, a1, t0
	inswl	t0, a0, t0
	mskwl	t2, a0, t2
	or	t2, t0, t0
	stq_c	t0, 0(t1)
	beq	t0, 1f
	mb
	RET
1:	br	0b
	END(atomic_add_16)

LEAF(atomic_subtract_16, 2)
	bic	a0, 7, t1
0:	ldq_l	t2, 0(t1)
	extwl	t2, a0, t0
	subl	t0, a1, t0
	inswl	t0, a0, t0
	mskwl	t2, a0, t2
	or	t2, t0, t0
	stq_c	t0, 0(t1)
	beq	t0, 1f
	mb
	RET
1:	br	0b
	END(atomic_subtract_16)
