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

ENTRY(bzero, 2)
	
	cmp.le	p6,p0=in1,r0			// bail if len <= 0
(p6)	br.ret.spnt.few rp
	;; 
	mov	r14=ar.lc			// save ar.lc

	cmp.ltu	p6,p0=17,in1			// check for small
(p6)	br.dptk.few 3f	

1:	add	r15=-1,in1 ;;
	mov	ar.lc=r15 ;;
2:	st1	[in0]=r0,1			// zero one byte
	br.cloop.sptk.few 2b			// loop

	;;
	mov	ar.lc=r14			// done
	br.ret.sptk.few rp

	// Zero up to 8byte alignment
	
3:	tbit.nz	p6,p0=in0,0 ;;
(p6)	st1	[in0]=r0,1
(p6)	add	in1=-1,in1 ;;

	tbit.nz	p6,p0=in0,1 ;;
(p6)	st2	[in0]=r0,2
(p6)	add	in1=-2,in1 ;;

	tbit.nz	p6,p0=in0,2 ;;
(p6)	st4	[in0]=r0,4
(p6)	add	in1=-4,in1

	;;
	shr.u	r15=in1,3			// word count
	extr.u	in1=in1,0,3 ;;			// trailing bytes
	cmp.eq	p6,p0=r15,r0			// check for zero
	cmp.ne	p7,p0=in1,r0
(p6)	br.dpnt.few 1b				// zero last bytes

	add	r15=-1,r15 ;;
	mov	ar.lc=r15 ;;
4:	st8	[in0]=r0,8
	br.cloop.sptk.few 4b 

(p7)	br.dpnt.few 1b				// zero last bytes

	;;
	mov	ar.lc=r14			// done
	br.ret.sptk.few rp

END(bzero)
