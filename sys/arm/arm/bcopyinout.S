/*	$NetBSD: bcopyinout.S,v 1.11 2003/10/13 21:22:40 scw Exp $	*/

/*-
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen Briggs for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "assym.inc"

#include <machine/asm.h>
#include <sys/errno.h>

.L_arm_memcpy:
	.word	_C_LABEL(_arm_memcpy)
.L_min_memcpy_size:
	.word	_C_LABEL(_min_memcpy_size)

__FBSDID("$FreeBSD$");
#include <arm/arm/bcopyinout_xscale.S>

/*
 * int badaddr_read_1(const uint8_t *src, uint8_t *dest)
 *
 * Copies a single 8-bit value from src to dest, returning 0 on success,
 * else EFAULT if a page fault occurred.
 */
ENTRY(badaddr_read_1)
	GET_PCB(r2)
	ldr	r2, [r2]

	ldr	ip, [r2, #PCB_ONFAULT]
	adr	r3, 1f
	str	r3, [r2, #PCB_ONFAULT]
	nop
	nop
	nop
	ldrb	r3, [r0]
	nop
	nop
	nop
	strb	r3, [r1]
	mov	r0, #0		/* No fault */
1:	str	ip, [r2, #PCB_ONFAULT]
	RET
END(badaddr_read_1)

/*
 * int badaddr_read_2(const uint16_t *src, uint16_t *dest)
 *
 * Copies a single 16-bit value from src to dest, returning 0 on success,
 * else EFAULT if a page fault occurred.
 */
ENTRY(badaddr_read_2)
	GET_PCB(r2)
	ldr	r2, [r2]

	ldr	ip, [r2, #PCB_ONFAULT]
	adr	r3, 1f
	str	r3, [r2, #PCB_ONFAULT]
	nop
	nop
	nop
	ldrh	r3, [r0]
	nop
	nop
	nop
	strh	r3, [r1]
	mov	r0, #0		/* No fault */
1:	str	ip, [r2, #PCB_ONFAULT]
	RET
END(badaddr_read_2)

/*
 * int badaddr_read_4(const uint32_t *src, uint32_t *dest)
 *
 * Copies a single 32-bit value from src to dest, returning 0 on success,
 * else EFAULT if a page fault occurred.
 */
ENTRY(badaddr_read_4)
	GET_PCB(r2)
	ldr	r2, [r2]

	ldr	ip, [r2, #PCB_ONFAULT]
	adr	r3, 1f
	str	r3, [r2, #PCB_ONFAULT]
	nop
	nop
	nop
	ldr	r3, [r0]
	nop
	nop
	nop
	str	r3, [r1]
	mov	r0, #0		/* No fault */
1:	str	ip, [r2, #PCB_ONFAULT]
	RET
END(badaddr_read_4)

