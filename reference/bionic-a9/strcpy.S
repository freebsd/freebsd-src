/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (c) 2008 ARM Ltd
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
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ARM LTD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ARM LTD BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Android adaptation and tweak by Jim Huang <jserv@0xlab.org>.
 */

	.globl strcpy
	.type strcpy, %function
	.text

strcpy:
	.fnstart
	PLD [r1, #0]
	eor	r2, r0, r1
	mov	ip, r0
	tst	r2, #3
	bne	4f
	tst	r1, #3
	bne	3f
5:
	str	r5, [sp, #-4]!
	mov	r5, #0x01
	orr	r5, r5, r5, lsl #8
	orr	r5, r5, r5, lsl #16

	str	r4, [sp, #-4]!
	tst	r1, #4
	ldr	r3, [r1], #4
	beq	2f
	sub	r2, r3, r5
	bics	r2, r2, r3
	tst	r2, r5, lsl #7
	itt	eq
	streq	r3, [ip], #4
	ldreq	r3, [r1], #4
	bne	1f
       /* Inner loop.  We now know that r1 is 64-bit aligned, so we
	  can safely fetch up to two words.  This allows us to avoid
	  load stalls.  */
	.p2align 2
2:
	PLD [r1, #8]
	ldr	r4, [r1], #4
	sub	r2, r3, r5
	bics	r2, r2, r3
	tst	r2, r5, lsl #7
	sub	r2, r4, r5
	bne	1f
	str	r3, [ip], #4
	bics	r2, r2, r4
	tst	r2, r5, lsl #7
	itt	eq
	ldreq	r3, [r1], #4
	streq	r4, [ip], #4
	beq	2b
	mov	r3, r4
1:
#ifdef __ARMEB__
	rors	r3, r3, #24
#endif
	strb	r3, [ip], #1
	tst	r3, #0xff
#ifdef __ARMEL__
	ror	r3, r3, #8
#endif
	bne	1b
	ldr	r4, [sp], #4
	ldr	r5, [sp], #4
	bx	lr

       /* Strings have the same offset from word alignment, but it's
	  not zero.  */
3:
	tst	r1, #1
	beq	1f
	ldrb	r2, [r1], #1
	strb	r2, [ip], #1
	cmp	r2, #0
	it	eq
	bxeq	lr
1:
	tst	r1, #2
	beq	5b
	ldrh	r2, [r1], #2
#ifdef __ARMEB__
	tst	r2, #0xff00
	iteet	ne
	strneh	r2, [ip], #2
	lsreq	r2, r2, #8
	streqb	r2, [ip]
	tstne	r2, #0xff
#else
	tst	r2, #0xff
	itet	ne
	strneh	r2, [ip], #2
	streqb	r2, [ip]
	tstne	r2, #0xff00
#endif
	bne	5b
	bx	lr

       /* src and dst do not have a common word-alignement.  Fall back to
	  byte copying.  */
4:
	ldrb	r2, [r1], #1
	strb	r2, [ip], #1
	cmp	r2, #0
	bne	4b
	bx	lr

	.fnend
