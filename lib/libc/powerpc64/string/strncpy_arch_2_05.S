/*-
 * Copyright (c) 2018 Instituto de Pesquisas Eldorado
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
 * 3. Neither the name of the author nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
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
ENTRY(__strncpy_arch_2_05)
	stdu	%r1,-40(%r1)
	mflr	%r0
	std	%r0,16(%r1)
	std	%r3,32(%r1)

	xor	%r6,%r6,%r6	/* fixed 0 reg */

/* align loop */
	addi	%r3,%r3,-1
.Lalign_loop:
	/* len? */
	cmpdi	%r5,0
	beq	.Lexit
	/* aligned? */
	andi.	%r0,%r4,7
	beq	.Ldw_copy
	/* copy */
	lbz	%r7,0(%r4)
	stbu	%r7,1(%r3)
	addi	%r4,%r4,1
	addi	%r5,%r5,-1
	/* zero? */
	cmpdi	%r7,0
	beq	.Lzero
	b	.Lalign_loop

/* dword copy loop */
.Ldw_copy:
	/* prepare src and dst to use load/store and update */
	addi	%r3,%r3,-7
	addi	%r4,%r4,-8
.Ldw_copy_loop:
	cmpdi	%r5,8
	blt	.Lbyte_copy

	ldu	%r0,8(%r4)
	/* check for 0 */
	cmpb	%r7,%r0,%r6
	cmpdi	%r7,0
	bne	.Lbyte_copy_and_zero
	/* copy to dst */
	stdu	%r0,8(%r3)
	addi	%r5,%r5,-8
	b	.Ldw_copy_loop

/* Copy remaining src bytes, zero-out buffer
 * Note: r5 will be >= 8
 */
.Lbyte_copy_and_zero:
	addi	%r3,%r3,7
	addi	%r4,%r4,-1
.Lbyte_copy_and_zero_loop:
	lbzu	%r7,1(%r4)
	stbu	%r7,1(%r3)
	addi	%r5,%r5,-1
	cmpdi	%r7,0
	beq	.Lzero
	b	.Lbyte_copy_and_zero_loop

/* zero-out remaining dst bytes */
.Lzero:
	addi	%r3,%r3,1
	li	%r4,0
	/* r5 has len already */
	bl	memset
	nop
	b	.Lexit

/* copy remaining (< 8) bytes */
.Lbyte_copy:
	cmpdi	%r5,0
	beq	.Lexit
	addi	%r3,%r3,7
	addi	%r4,%r4,7
	mtctr	%r5
.Lbyte_copy_loop:
	lbzu	%r7,1(%r4)
	stbu	%r7,1(%r3)
	cmpdi	%r7,0
	/* 0 found: zero out remaining bytes */
	beq	.Lbyte_copy_zero
	bdnz	.Lbyte_copy_loop
	b	.Lexit
.Lbyte_copy_zero_loop:
	stbu	%r6,1(%r3)
.Lbyte_copy_zero:
	bdnz	.Lbyte_copy_zero_loop

.Lexit:
	/* epilogue */
	ld	%r3,32(%r1)
	ld	%r0,16(%r1)
	mtlr	%r0
	addi	%r1,%r1,40
	blr

END(__strncpy_arch_2_05)

	.section .note.GNU-stack,"",%progbits
