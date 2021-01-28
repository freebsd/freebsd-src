/*	$NetBSD: ffs.S,v 1.5 2003/04/05 23:08:52 bjh21 Exp $	*/
/*
 * Copyright (c) 2001 Christopher Gilbert
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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD$");

.syntax unified

/*
 * ffs - find first set bit, this algorithm isolates the first set
 * bit, then multiplies the number by 0x0450fbaf which leaves the top
 * 6 bits as an index into the table.  This algorithm should be a win
 * over the checking each bit in turn as per the C compiled version.
 *
 * This is the ffs algorithm devised by d.seal and posted to comp.sys.arm on
 * 16 Feb 1994.
 */

ENTRY(ffs)
	/* Standard trick to isolate bottom bit in r0 or 0 if r0 = 0 on entry */
 	rsb     r1, r0, #0
 	ands    r0, r0, r1
	itt	ne
	clzne	r0, r0
	rsbne	r0, r0, #32
	RET
END(ffs)

	.section .note.GNU-stack,"",%progbits
