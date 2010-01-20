/*	$NetBSD: ffs.S,v 1.3 1996/10/17 03:08:13 cgd Exp $	*/

/*
 * Copyright (c) 1995 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

ENTRY(ffs, 1)
	sxt4	r14=in0 ;;
	cmp.eq	p6,p0=r14,r0
(p6)	br.dpnt.few Lallzero
	
	/*
	 * Initialize return value (ret0), and set up r15 so that it
	 * contains the mask with only the lowest bit set.
	 */
	sub	r15=r0,r14
	mov	ret0=1 ;;
	and	r15=r14,r15 ;;

	extr.u	r16=r15,0,8 ;;
	cmp.ne	p6,p0=r0,r16
(p6)	br.dptk.few Ldo8

	/*
	 * If lower 16 bits empty, add 16 to result and use upper 16.
	 */
	extr.u	r16=r15,0,16 ;;
	cmp.ne	p6,p0=r0,r16
(p6)	br.dptk.few Ldo16
	extr.u	r15=r15,16,16
	add	ret0=16,ret0 ;;
	
Ldo16:
	/*
	 * If lower 8 bits empty, add 8 to result and use upper 8.
	 */
	extr.u	r16=r15,0,8 ;;
	cmp.ne	p6,p0=r0,r16
(p6)	br.dptk.few Ldo8
	extr.u	r15=r15,8,24
	add	ret0=8,ret0 ;;

Ldo8:
	and	r16=0x0f,r15		/* lower 4 of 8 empty? */
	and	r17=0x33,r15		/* lower 2 of each 4 empty? */
	and	r18=0x55,r15 ;;		/* lower 1 of each 2 empty? */
	cmp.ne	p6,p0=r16,r0
	cmp.ne	p7,p0=r17,r0
	cmp.ne	p8,p0=r18,r0

	/* If lower 4 bits empty, add 4 to result. */
(p6)	br.dptk.few Ldo4
	add	ret0=4,ret0 ;;

Ldo4:	/* If lower 2 bits of each 4 empty, add 2 to result. */
(p7)	br.dptk.few Ldo2
	add	ret0=2,ret0 ;;

Ldo2:	/* If lower bit of each 2 empty, add 1 to result. */
(p8)	br.dptk.few Ldone
	add	ret0=1,ret0

Ldone:
	br.ret.sptk.few rp

Lallzero:
	mov	ret0=0
	br.ret.sptk.few rp
END(ffs)
