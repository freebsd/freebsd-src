/*-
 * Copyright (C) 2003 Jake Burkholder <jake@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
__FBSDID("$FreeBSD$");

#include "assym.s"

	.weak	CNAME(_thr_getcontext)
	.set	CNAME(_thr_getcontext),CNAME(__thr_getcontext)
ENTRY(__thr_getcontext)
	add	%o7, 8, %o1
	add	%o1, 4, %o2
	stx	%sp, [%o0 + MC_OUT + (6 * 8)]
	stx	%o1, [%o0 + MC_TPC]
	stx	%o2, [%o0 + MC_TNPC]
	mov	1, %l0			# Validate the context.
	stx	%l0, [%o0 + MC_GLOBAL]
	retl
	nop
END(__thr_getcontext)

	.weak	CNAME(_thr_setcontext)
	.set	CNAME(_thr_setcontext),CNAME(__thr_setcontext)
ENTRY(__thr_setcontext)
	save	%sp, -CCFSZ, %sp
	flushw
	mov	%i0, %l0
	mov	%i1, %l1
	mov	%i2, %l2
	ldx	[%l0 + MC_GLOBAL + (1 * 8)], %g1
	ldx	[%l0 + MC_GLOBAL + (2 * 8)], %g2
	ldx	[%l0 + MC_GLOBAL + (3 * 8)], %g3
	ldx	[%l0 + MC_GLOBAL + (4 * 8)], %g4
	ldx	[%l0 + MC_GLOBAL + (5 * 8)], %g5
	ldx	[%l0 + MC_GLOBAL + (6 * 8)], %g6
	ldx	[%l0 + MC_GLOBAL + (7 * 8)], %g7
	ldx	[%l0 + MC_OUT + (0 * 8)], %i0
	ldx	[%l0 + MC_OUT + (1 * 8)], %i1
	ldx     [%l0 + MC_OUT + (2 * 8)], %i2
	ldx	[%l0 + MC_OUT + (3 * 8)], %i3
	ldx	[%l0 + MC_OUT + (4 * 8)], %i4
	ldx	[%l0 + MC_OUT + (5 * 8)], %i5
	ldx	[%l0 + MC_OUT + (6 * 8)], %i6
	ldx	[%l0 + MC_OUT + (7 * 8)], %i7
	ldx	[%l0 + MC_TPC], %l4
	ldx	[%l0 + MC_TNPC], %l3
	brz	%l2, 1f
	nop
	stx	%l1, [%l2]
1:	jmpl	%l3, %g0
	return	%l4
END(__thr_setcontext)

ENTRY(_sparc64_enter_uts)
	save	%sp, -CCFSZ, %sp
	flushw
	add	%i2, %i3, %i2
	sub	%i2, SPOFF + CCFSZ, %sp
	jmpl	%i1, %g0
	mov	%i0, %o0
END(_sparc64_enter_uts)
