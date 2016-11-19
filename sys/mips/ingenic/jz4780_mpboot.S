/*-
 * Copyright (c) 2015 Alexander Kabaev
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
#include <machine/cpu.h>
#include <machine/cpuregs.h>
#include <machine/cache_r4k.h>

#include "assym.s"

#define CACHE_SIZE (32 * 1024)
#define CACHE_LINESIZE 32

	.text
	.set	noat
	.set	noreorder
	.section .text.mpentry_jz4780
	.balign 0x10000

GLOBAL(jz4780_mpentry)

	/* Initialize caches */
	li	t0, MIPS_KSEG0_START
	ori	t1, t0, CACHE_SIZE
	mtc0	zero, MIPS_COP_0_TAG_LO
	COP0_SYNC
1:	cache	CACHEOP_R4K_INDEX_STORE_TAG | CACHE_R4K_I, 0(t0)
	cache	CACHEOP_R4K_INDEX_STORE_TAG | CACHE_R4K_D, 0(t0)
	bne	t0, t1, 1b
	addiu	t0, t0, CACHE_LINESIZE

	/* Set TLB page mask */
	mtc0	zero, MIPS_COP_0_TLB_PG_MASK
	COP0_SYNC

	j	mpentry
	nop
