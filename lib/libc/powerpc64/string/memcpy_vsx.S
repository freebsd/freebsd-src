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

#define FN_NAME			__memcpy_vsx
#define BLOCK_BITS		6
#define	ALIGN_MASK		0xf

/*
 * r5: bytes to copy (multiple of BLOCK_BYTES)
 *
 */
#define FN_COPY_LOOP \
	/* Load CTR with number of blocks */		 \
	srdi	%r5, %r5, BLOCK_BITS			;\
	mtctr	%r5					;\
	/* Prepare indexes to load and store data */	 \
	xor	%r6, %r6, %r6				;\
	li	%r9,  16				;\
	li	%r10, 32				;\
	li	%r11, 48				;\
.Lcopy_vsx_loop:					 \
	lxvd2x	%vs6, %r6,  %r4				;\
	lxvd2x	%vs7, %r9,  %r4				;\
	lxvd2x	%vs8, %r10, %r4				;\
	lxvd2x	%vs9, %r11, %r4				;\
	stxvd2x	%vs6, %r6,  %r3				;\
	stxvd2x	%vs7, %r9,  %r3				;\
	stxvd2x	%vs8, %r10, %r3				;\
	stxvd2x	%vs9, %r11, %r3				;\
							 \
	addi	%r3, %r3, BLOCK_BYTES			;\
	addi	%r4, %r4, BLOCK_BYTES			;\
	bdnz	.Lcopy_vsx_loop				;\
							 \
	/* Check if there is remaining bytes */		 \
	cmpd	%r7, 0					;\
	beq		.Lexit				;\

#include "memcpy.S"
