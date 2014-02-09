/*-
 * Copyright (c) 2007 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <machine/asm.h>

	.text

/*
 * uint32_t atomic_cas_32(volatile uint32_t *p, uint32_t cmp, uint32_t v)
 */
ENTRY(atomic_cas_32, 3)
	mov             ar.ccv = r33
	;;
	cmpxchg4.acq    r8 = [r32], r34, ar.ccv
	;;
	br.ret.sptk     rp
END(atomic_cas_32)

/*
 * uint64_t atomic_cas_64(volatile uint64_t *p, uint64_t cmp, uint64_t v)
 */
ENTRY(atomic_cas_64, 3)
	mov             ar.ccv = r33
	;;
	cmpxchg8.acq    r8 = [r32], r34, ar.ccv
	;;
	br.ret.sptk     rp
END(atomic_cas_64)

/*
 * uint64_t atomic_add_64_nv(volatile uint64_t *p, uint64_t v)
 */
ENTRY(atomic_add_64_nv, 2)
1:
	ld8		r16 = [r32]
	;;
	mov		ar.ccv = r16
	add		r8 = r16, r33
	;;
	cmpxchg8.acq	r17 = [r32], r8, ar.ccv
	;;
	cmp.eq		p6, p7 = r16, r17
(p6)	br.ret.sptk	rp
(p7)	br.cond.spnt	1b
END(atomic_add_64_nv)

/*
 * uint8_t atomic_or_8_nv(volatile uint8_t *p, uint8_t v)
 */
ENTRY(atomic_or_8_nv, 2)
1:
	ld1		r16 = [r32]
	;;
	mov		ar.ccv = r16
	or		r8 = r16, r33
	;;
	cmpxchg1.acq	r17 = [r32], r8, ar.ccv
	;;
	cmp.eq		p6, p7 = r16, r17
(p6)	br.ret.sptk	rp
(p7)	br.cond.spnt	1b
END(atomic_or_8_nv)

ENTRY(membar_producer, 0)
	mf
	;;
	br.ret.sptk	rp
END(membar_producer)
