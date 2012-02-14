/*
 * Copyright (c) 2004 Marcel Moolenaar
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

	.text

/*
 * Important registers:
 *	r8	structure return address
 *	r15	static link (nested routines)
 *	rp	our return address
 *	in0	caller's ar.pfs
 *	in1	caller's gp
 *	in2	caller's rp
 *	in3	GOT entry
 *	ar.pfs	our pfs
 */
ENTRY_NOPROFILE(_mcount, 4)
	alloc		loc0 = ar.pfs, 4, 4, 2, 0
	mov		loc1 = r8
	mov		loc2 = rp
	mov		loc3 = r15
	;;
	mov		out0 = in2
	mov		out1 = rp
	br.call.sptk	rp = __mcount
	;;
1:
	mov		gp = in1
	mov		r14 = ip
	mov		b7 = loc2
	;;
	add		r14 = 2f - 1b, r14
	mov		ar.pfs = loc0
	mov		rp = in2
	;;
	mov		r15 = loc3
	mov		b7 = r14
	mov		b6 = loc2
	mov		r8 = loc1
	mov		r14 = in0
	br.ret.sptk	b7
	;;
2:
	mov		ar.pfs = r14
	br.sptk		b6
	;;
END(_mcount)

WEAK_ALIAS(mcount, _mcount)
