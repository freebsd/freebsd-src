/*
 *  polynomial.S
 *
 * Fixed point arithmetic polynomial evaluation.
 *
 * Call from C as:
 *   void polynomial(unsigned accum[], unsigned x[], unsigned terms[][2],
 *                   int n)
 *
 * Computes:
 * terms[0] + (terms[1] + (terms[2] + ... + (terms[n-1]*x)*x)*x)*x) ... )*x
 * The result is returned in accum.
 *
 *
 * Copyright (C) 1992, 1993  W. Metzenthen, 22 Parker St, Ormond,
 *                           Vic 3163, Australia.
 *                           E-mail apm233m@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD operating system. Any other use is not permitted
 * under this copyright.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must include information specifying
 *    that source code for the emulator is freely available and include
 *    either:
 *      a) an offer to provide the source code for a nominal distribution
 *         fee, or
 *      b) list at least two alternative methods whereby the source
 *         can be obtained, e.g. a publically accessible bulletin board
 *         and an anonymous ftp site from which the software can be
 *         downloaded.
 * 3. All advertising materials specifically mentioning features or use of
 *    this emulator must acknowledge that it was developed by W. Metzenthen.
 * 4. The name of W. Metzenthen may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * W. METZENTHEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

	.file	"fpolynom.s"

#include "fpu_asm.h"


/*	#define	EXTRA_PRECISE*/

#define	TERM_SIZE	$8


.text
	.align 2,144
.globl _polynomial
_polynomial:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$32,%esp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi		/* accum */
	movl	PARAM2,%edi		/* x */
	movl	PARAM3,%ebx		/* terms */
	movl	PARAM4,%ecx		/* n */

	movl	TERM_SIZE,%eax
	mull	%ecx
	movl	%eax,%ecx

	movl	4(%ebx,%ecx,1),%edx	/* terms[n] */
	movl	%edx,-20(%ebp)
	movl	(%ebx,%ecx,1),%edx	/* terms[n] */
	movl	%edx,-24(%ebp)		
	xor	%eax,%eax
	movl	%eax,-28(%ebp)

	subl	TERM_SIZE,%ecx
	js	L_accum_done

L_accum_loop:
	xor	%eax,%eax
	movl	%eax,-4(%ebp)
	movl	%eax,-8(%ebp)

#ifdef EXTRA_PRECISE
	movl	-28(%ebp),%eax
	mull	4(%edi)			/* x ms long */
	movl	%edx,-12(%ebp)
#endif EXTRA_PRECISE

	movl	-24(%ebp),%eax
	mull	(%edi)			/* x ls long */
/*	movl	%eax,-16(%ebp)	*/	/* Not needed */
	addl	%edx,-12(%ebp)
	adcl	$0,-8(%ebp)

	movl	-24(%ebp),%eax
	mull	4(%edi)			/* x ms long */
	addl	%eax,-12(%ebp)
	adcl	%edx,-8(%ebp)
	adcl	$0,-4(%ebp)

	movl	-20(%ebp),%eax
	mull	(%edi)
	addl	%eax,-12(%ebp)
	adcl	%edx,-8(%ebp)
	adcl	$0,-4(%ebp)

	movl	-20(%ebp),%eax
	mull	4(%edi)
	addl	%eax,-8(%ebp)
	adcl	%edx,-4(%ebp)

/* Now add the next term */
	movl	(%ebx,%ecx,1),%eax
	addl	%eax,-8(%ebp)
	movl	4(%ebx,%ecx,1),%eax
	adcl	%eax,-4(%ebp)

/* And put into the second register */
	movl	-4(%ebp),%eax
	movl	%eax,-20(%ebp)
	movl	-8(%ebp),%eax
	movl	%eax,-24(%ebp)

#ifdef EXTRA_PRECISE
	movl	-12(%ebp),%eax
	movl	%eax,-28(%ebp)
#else
	testb	$128,-25(%ebp)
	je	L_no_poly_round

	addl	$1,-24(%ebp)
	adcl	$0,-20(%ebp)
L_no_poly_round:
#endif EXTRA_PRECISE

	subl	TERM_SIZE,%ecx
	jns	L_accum_loop

L_accum_done:
#ifdef EXTRA_PRECISE
/* And round the result */
	testb	$128,-25(%ebp)
	je	L_poly_done

	addl	$1,-24(%ebp)
	adcl	$0,-20(%ebp)
#endif EXTRA_PRECISE

L_poly_done:
	movl	-24(%ebp),%eax
	movl	%eax,(%esi)
	movl	-20(%ebp),%eax
	movl	%eax,4(%esi)

	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret
