/*-
 * Copyright (c) 2014 Andrew Turner
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
 */

/*
 * Interrupt counters for vmstat.
 * TODO: These could live in a .c file.
 */
	.data
	.globl intrcnt
	.globl sintrcnt
	.globl intrnames
	.globl sintrnames

intrnames:
	.space	8
sintrnames:
	.quad	8
intrcnt:
	.space	8 * 8 /* u_long[8] */
sintrcnt:
	.quad	8

	.text
#define	EMIT(x)			\
	mov	x0, x;		\
	mov	x1, 0x1c090000; \
	str	x0, [x1];

.macro	vempty
	.align 7
	EMIT('Z');
	1: b	1b
.endm

.macro	el1h_sync
	.align 7
	EMIT('B');
	1: b	1b
.endm

.macro	el1h_error
	.align 7
	EMIT('C');
	1: b	1b
.endm

	.align 11
	.globl exception_vectors
exception_vectors:
	vempty		/* Synchronous EL1t */
	vempty		/* IRQ EL1t */
	vempty		/* FIQ EL1t */
	vempty		/* Error EL1t */

	el1h_sync	/* Synchronous EL1h */
	vempty		/* IRQ EL1h */
	vempty		/* FIQ EL1h */
	el1h_error	/* Error EL1h */

	vempty		/* Synchronous 64-bit EL0 */
	vempty		/* IRQ 64-bit EL0 */
	vempty		/* FIQ 64-bit EL0 */
	vempty		/* Error 64-bit EL0 */

	vempty		/* Synchronous 32-bit EL0 */
	vempty		/* IRQ 32-bit EL0 */
	vempty		/* FIQ 32-bit EL0 */
	vempty		/* Error 32-bit EL0 */

