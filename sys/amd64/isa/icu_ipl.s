/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

	.data
	ALIGN_DATA
vec:
	.long	 vec0,  vec1,  vec2,  vec3,  vec4,  vec5,  vec6,  vec7
	.long	 vec8,  vec9, vec10, vec11, vec12, vec13, vec14, vec15

/* interrupt mask enable (all h/w off) */
	.globl	_imen
_imen:	.long	HWI_MASK


/*
 * 
 */
	.text
	SUPERALIGN_TEXT

/*
 * Fake clock interrupt(s) so that they appear to come from our caller instead
 * of from here, so that system profiling works.
 * XXX do this more generally (for all vectors; look up the C entry point).
 * XXX frame bogusness stops us from just jumping to the C entry point.
 */
	ALIGN_TEXT
vec0:
	popl	%eax			/* return address */
	pushfl
	pushl	$KCSEL
	pushl	%eax
	cli
	MEXITCOUNT
	jmp	_Xintr0			/* XXX might need _Xfastintr0 */

#ifndef PC98
	ALIGN_TEXT
vec8:
	popl	%eax	
	pushfl
	pushl	$KCSEL
	pushl	%eax
	cli
	MEXITCOUNT
	jmp	_Xintr8			/* XXX might need _Xfastintr8 */
#endif /* PC98 */

/*
 * The 'generic' vector stubs.
 */

#define BUILD_VEC(irq_num)			\
	ALIGN_TEXT ;				\
__CONCAT(vec,irq_num): ;			\
	int	$ICU_OFFSET + (irq_num) ;	\
	ret

	BUILD_VEC(1)
	BUILD_VEC(2)
	BUILD_VEC(3)
	BUILD_VEC(4)
	BUILD_VEC(5)
	BUILD_VEC(6)
	BUILD_VEC(7)
#ifdef PC98
	BUILD_VEC(8)
#endif
	BUILD_VEC(9)
	BUILD_VEC(10)
	BUILD_VEC(11)
	BUILD_VEC(12)
	BUILD_VEC(13)
	BUILD_VEC(14)
	BUILD_VEC(15)
