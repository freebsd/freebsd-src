/*
 * Copyright (c) 1999, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD: src/sys/boot/arc/lib/arch/alpha/start.S,v 1.2 1999/08/28 00:39:41 peter Exp $
 *
 */

/*
 * Based on /sys/boot/alpha/libalpha/start.S
 * Copyright (c) 1992 Carnegie Mellon University
 */

#include <machine/asm.h>

	.text

#define ENTRY_FRAME	32

NESTED(_start, 1, ENTRY_FRAME, ra, 0, 0)
	br	pv,Lstartgp
Lstartgp:
	LDGP(pv)

	lda	a0,_edata
	lda	a1,_end
	subq	a1,a0,a1
	CALL(bzero)

	lda	sp, -8(sp)
	stq	ra, 0(sp)
	
	CALL(main)			/* transfer to C */

	ldq	ra, 0(sp)
	lda	sp, 8(sp)
	RET /* XXX */

XLEAF(_rtt, 0)
XLEAF(halt, 0)
	call_pal PAL_halt		/* halt if we ever return */
END(_start)
