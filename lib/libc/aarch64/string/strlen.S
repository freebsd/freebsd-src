/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Getz Mikalsen <getz@FreeBSD.org>
*/

#include <machine/asm.h>

	.weak	strlen
	.set	strlen, __strlen
	.text

ENTRY(__strlen)
	bic	x10, x0, #0xf		// aligned src
	and	x9, x0, #0xf
	ldr	q0, [x10]
	cmeq	v0.16b, v0.16b, #0
	shrn	v0.8b, v0.8h, #4
	fmov	x1, d0
	cbz	x9, .Laligned
	lsl	x2, x0, #2		// get the byte offset
	lsr	x1, x1, x2		// shift by offset index
	cbz	x1, .Lloop
	rbit	x1, x1
	clz	x0, x1
	lsr	x0, x0, #2
	ret

.Laligned:
	cbnz	x1, .Ldone

.Lloop:
	ldr	q0, [x10, #16]!
	cmeq	v0.16b, v0.16b, #0
	shrn	v0.8b, v0.8h, #4	// reduce to fit mask in GPR
	fmov	x1, d0
	cbz	x1, .Lloop
.Ldone:
	sub	x0, x10, x0
	rbit	x1, x1			// reverse bits as NEON has no ctz
	clz	x3, x1
	lsr	x3, x3, #2
	add	x0, x0, x3
	ret
END(__strlen)
