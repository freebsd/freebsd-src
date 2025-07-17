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

#ifndef FN_NAME
#define FN_NAME		__bcopy_vsx
#endif

/*
 * r3: dst
 * r4: src
 * r5: block increment
 * r6: blocks to copy
 * r7/r8/r9/r10: 16-byte offsets to copy
 */

#define FN_PHASE2 \
	mtctr	%r6			;\
	.align 5			;\
.Lphase2_loop:				;\
	lxvd2x  %vs6, %r7,  %r4		;\
	lxvd2x  %vs7, %r8,  %r4		;\
	lxvd2x  %vs8, %r9,  %r4		;\
	lxvd2x  %vs9, %r10, %r4		;\
	stxvd2x %vs6, %r7,  %r3		;\
	stxvd2x %vs7, %r8,  %r3		;\
	stxvd2x %vs8, %r9,  %r3		;\
	stxvd2x %vs9, %r10, %r3		;\
	/* phase 2 increment */		;\
	add	%r4, %r4, %r5		;\
	add	%r3, %r3, %r5		;\
					 \
	bdnz	.Lphase2_loop		;\

#include "bcopy.S"
