/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#include <machine/asm.h>

/*
 * Not the fastest bcopy in the world.
 */
ENTRY(bcopy, 3)

	cmp.le	p6,p0=in2,r0			// bail if len <= 0
(p6)	br.ret.spnt.few rp

	sub	r14=in1,in0 ;;			// check for overlap
	cmp.ltu	p6,p0=r14,in2			// dst-src < len
(p6)	br.cond.spnt.few 5f

	extr.u	r14=in0,0,3			// src & 7
	extr.u	r15=in1,0,3 ;;			// dst & 7
	cmp.eq	p6,p0=r14,r15			// different alignment?
(p6)	br.cond.spnt.few 2f			// branch if same alignment

1:	ld1	r14=[in0],1 ;;			// copy bytewise
	st1	[in1]=r14,1
	add	in2=-1,in2 ;;			// len--
	cmp.ne	p6,p0=r0,in2
(p6)	br.cond.dptk.few 1b			// loop
	br.ret.sptk.few rp			// done

2:	cmp.eq	p6,p0=r14,r0			// aligned?
(p6)	br.cond.sptk.few 4f

3:	ld1	r14=[in0],1 ;;			// copy bytewise
	st1	[in1]=r14,1
	extr.u	r15=in0,0,3			// src & 7
	add	in2=-1,in2 ;;			// len--
	cmp.eq	p6,p0=r0,in2			// done?
	cmp.eq	p7,p0=r0,r15 ;;			// aligned now?
(p6)	br.ret.spnt.few rp			// return if done
(p7)	br.cond.spnt.few 4f			// go to main copy
	br.cond.sptk.few 3b			// more bytes to copy

	// At this point, in2 is non-zero

4:	mov	r14=8 ;;
	cmp.ltu	p6,p0=in2,r14 ;;		// len < 8?
(p6)	br.cond.spnt.few 1b			// byte copy the end
	ld8	r15=[in0],8 ;;			// copy word
	st8	[in1]=r15,8
	add	in2=-8,in2 ;;			// len -= 8
	cmp.ne	p6,p0=r0,in2			// done?
(p6)	br.cond.spnt.few 4b			// again

	br.ret.sptk.few rp			// return

	// Don't bother optimising overlap case

5:	add	in0=in0,in2
	add	in1=in1,in2 ;;
	add	in0=-1,in0
	add	in1=-1,in1 ;;

6:	ld1	r14=[in0],-1 ;;
	st1	[in1]=r14,-1
	add	in2=-1,in2 ;;
	cmp.ne	p6,p0=r0,in2
(p6)	br.cond.spnt.few 6b

	br.ret.sptk.few rp

END(bcopy)
