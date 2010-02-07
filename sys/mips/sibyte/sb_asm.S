/*-
 * Copyright (c) 2009 Neelkanth Natu
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

/*
 * We compile a 32-bit kernel to run on the SB-1 processor which is a 64-bit
 * processor. It has some registers that must be accessed using 64-bit load
 * and store instructions.
 *
 * So we have to resort to assembly because the compiler does not emit the
 * 'ld' and 'sd' instructions since it thinks that it is compiling for a
 * 32-bit mips processor.
 */

.set	mips64
.set	noat
.set	noreorder

/*
 * Parameters:		uint32_t ptr
 * Return value: 	*(uint64_t *)ptr
 */
LEAF(sb_load64)
	ld      v1, 0(a0)	/* result = *(uint64_t *)ptr */
	move	v0, v1
#if defined(TARGET_BIG_ENDIAN)
	dsll32	v1, v1, 0
	dsrl32	v1, v1, 0	/* v1 = lower_uint32(result) */
	jr	ra
	dsrl32	v0, v0, 0	/* v0 = upper_uint32(result) */
#else
	dsll32	v0, v0, 0
	dsrl32	v0, v0, 0	/* v0 = lower_uint32(result) */
	jr	ra
	dsrl32	v1, v1, 0	/* v1 = upper_uint32(result) */
#endif
END(sb_load64)

/*
 * Parameters:		uint32_t ptr, uint64_t val
 * Return value:	void
 */
LEAF(sb_store64)
#if defined(TARGET_BIG_ENDIAN)
	dsll32	a2, a2, 0	/* a2 = upper_uint32(val) */
	dsll32	a3, a3, 0	/* a3 = lower_uint32(val) */
	dsrl32	a3, a3, 0
#else
	dsll32	a3, a3, 0	/* a3 = upper_uint32(val) */
	dsll32	a2, a2, 0	/* a2 = lower_uint32(val) */
	dsrl32	a2, a2, 0
#endif
	or	t0, a2, a3
	jr	ra
	sd	t0, 0(a0)
END(sb_store64)
