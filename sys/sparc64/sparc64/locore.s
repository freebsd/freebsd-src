/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#include <machine/asmacros.h>

#include "assym.s"

/*
 * void _start(struct bootinfo *bi, u_long ofw_vec)
 */
ENTRY(_start)
	wrpr	%g0, PSTATE_IE|PSTATE_PRIV, %pstate
	mov	%o0, %g1
	mov	%o1, %g2
	flushw
	wrpr	%g0, 1, %cwp
	wrpr	%g0, 0, %cleanwin

	setx	user0 + UPAGES * PAGE_SIZE - SPOFF, %l0, %o5
	save	%o5, -CCFSZ, %sp

	mov	%g1, %o0
	call	sparc64_init
	 mov	%g2, %o1
	call	mi_startup
	 nop
	! NOTREACHED
END(_start)

ENTRY(sigcode)
	illtrap
esigcode:
END(sigcode)

DATA(szsigcode)
	.long	esigcode - sigcode
