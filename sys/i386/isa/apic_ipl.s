/*
 * Copyright (c) 1997, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *	$Id: apic_ipl.s,v 1.1 1997/07/18 22:54:17 smp Exp smp $
 */

	.data
	ALIGN_DATA

/* this allows us to change the 8254 APIC pin# assignment */
	.globl _Xintr8254
_Xintr8254:
	.long	_Xintr7

/* used by this file, microtime.s and clock.c */
	.globl _mask8254
_mask8254:
	.long	0

#ifdef DO_RTC_VEC
/** XXX FIXME: remove me after several weeks of no problems */
/* this allows us to change the RTC clock APIC pin# assignment */
	.globl _XintrRTC
_XintrRTC:
	.long	_Xintr7

/* used by this file and clock.c */
	.globl _maskRTC
_maskRTC:
	.long	0
#endif /* DO_RTC_VEC */

/*  */
	.globl _vec
_vec:
	.long	 vec0,  vec1,  vec2,  vec3,  vec4,  vec5,  vec6,  vec7
	.long	 vec8,  vec9, vec10, vec11, vec12, vec13, vec14, vec15
	.long	vec16, vec17, vec18, vec19, vec20, vec21, vec22, vec23


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

/*
 * generic vector function for 8254 clock
 */
	ALIGN_TEXT
	.globl	_vec8254
_vec8254:
	popl	%eax			/* return address */
	pushfl
	pushl	$KCSEL
	pushl	%eax
	cli
	movl	_mask8254,%eax		/* lazy masking */
	notl	%eax
	andl	%eax,iactive
	MEXITCOUNT
	movl	_Xintr8254, %eax
	jmp	%eax			/* XXX might need _Xfastintr# */

/*
 * generic vector function for RTC clock
 */
	ALIGN_TEXT
#ifdef DO_RTC_VEC

	.globl	_vecRTC
_vecRTC:
	popl	%eax	
	pushfl
	pushl	$KCSEL
	pushl	%eax
	cli
	movl	_maskRTC,%eax		/* lazy masking */
	notl	%eax
	andl	%eax, iactive
	MEXITCOUNT
	movl	_XintrRTC, %eax
	jmp	%eax			/* XXX might need _Xfastintr# */

#else /* DO_RTC_VEC */

vec8:
	popl	%eax	
	pushfl
	pushl	$KCSEL
	pushl	%eax
	cli
	andl	$~IRQ_BIT(8), iactive ;	/* lazy masking */
	MEXITCOUNT
	jmp	_Xintr8			/* XXX might need _Xfastintr8 */

#endif /* DO_RTC_VEC */


/*
 * The 'generic' vector stubs.
 */

#define BUILD_VEC(irq_num)						\
	ALIGN_TEXT ;							\
__CONCAT(vec,irq_num): ;						\
	popl	%eax ;							\
	pushfl ;							\
	pushl	$KCSEL ;						\
	pushl	%eax ;							\
	cli ;								\
	andl	$~IRQ_BIT(irq_num), iactive ;	/* lazy masking */	\
	MEXITCOUNT ;							\
	jmp	__CONCAT(_Xintr,irq_num)


	BUILD_VEC(0)			/* NOT specific in IO APIC hardware */
	BUILD_VEC(1)
	BUILD_VEC(2)
	BUILD_VEC(3)
	BUILD_VEC(4)
	BUILD_VEC(5)
	BUILD_VEC(6)
	BUILD_VEC(7)
#ifdef DO_RTC_VEC
	BUILD_VEC(8)
#endif /* DO__RTC_VEC */
	BUILD_VEC(9)
	BUILD_VEC(10)
	BUILD_VEC(11)
	BUILD_VEC(12)
	BUILD_VEC(13)
	BUILD_VEC(14)
	BUILD_VEC(15)
	BUILD_VEC(16)			/* 8 additional INTs in IO APIC */
	BUILD_VEC(17)
	BUILD_VEC(18)
	BUILD_VEC(19)
	BUILD_VEC(20)
	BUILD_VEC(21)
	BUILD_VEC(22)
	BUILD_VEC(23)

