/*-
 * Copyright (c) 2018 The FreeBSD Foundation
 *
 * This software was developed by Mateusz Guzik <mjg@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#define	ALIGN_TEXT	.p2align 4,0x90 /* 16-byte alignment, nop filled */

/*
 * memmove(dst, src, cnt)
 *         rdi, rsi, rdx
 * Contains parts of bcopy written by:
 *  ws@tools.de     (Wolfgang Solfrank, TooLs GmbH) +49-228-985800
 */

/*
 * Register state at entry is supposed to be as follows:
 * rdi - destination
 * rsi - source
 * rdx - count
 *
 * The macro possibly clobbers the above and: rcx, r8.
 * It does not clobber rax, r10 nor r11.
 */
.macro MEMMOVE erms overlap begin end
	\begin
.if \overlap == 1
	movq	%rdi,%r8
	subq	%rsi,%r8
	cmpq	%rcx,%r8	/* overlapping && src < dst? */
	jb	2f
.endif

	cmpq	$32,%rcx
	jb	1016f

	cmpq	$256,%rcx
	ja	1256f

1032:
	movq	(%rsi),%rdx
	movq	%rdx,(%rdi)
	movq	8(%rsi),%rdx
	movq	%rdx,8(%rdi)
	movq	16(%rsi),%rdx
	movq	%rdx,16(%rdi)
	movq	24(%rsi),%rdx
	movq	%rdx,24(%rdi)
	leaq	32(%rsi),%rsi
	leaq	32(%rdi),%rdi
	subq	$32,%rcx
	cmpq	$32,%rcx
	jae	1032b
	cmpb	$0,%cl
	jne	1016f
	\end
	ret
	ALIGN_TEXT
1016:
	cmpb	$16,%cl
	jl	1008f
	movq	(%rsi),%rdx
	movq	%rdx,(%rdi)
	movq	8(%rsi),%rdx
	movq	%rdx,8(%rdi)
	subb	$16,%cl
	jz	1000f
	leaq	16(%rsi),%rsi
	leaq	16(%rdi),%rdi
1008:
	cmpb	$8,%cl
	jl	1004f
	movq	(%rsi),%rdx
	movq	%rdx,(%rdi)
	subb	$8,%cl
	jz	1000f
	leaq	8(%rsi),%rsi
	leaq	8(%rdi),%rdi
1004:
	cmpb	$4,%cl
	jl	1002f
	movl	(%rsi),%edx
	movl	%edx,(%rdi)
	subb	$4,%cl
	jz	1000f
	leaq	4(%rsi),%rsi
	leaq	4(%rdi),%rdi
1002:
	cmpb	$2,%cl
	jl	1001f
	movw	(%rsi),%dx
	movw	%dx,(%rdi)
	subb	$2,%cl
	jz	1000f
	leaq	2(%rsi),%rsi
	leaq	2(%rdi),%rdi
1001:
	cmpb	$1,%cl
	jl	1000f
	movb	(%rsi),%dl
	movb	%dl,(%rdi)
1000:
	\end
	ret

	ALIGN_TEXT
1256:
.if \erms == 1
	rep
	movsb
.else
	shrq	$3,%rcx                         /* copy by 64-bit words */
	rep
	movsq
	movq	%rdx,%rcx
	andb	$7,%cl                         /* any bytes left? */
	jne	1004b
.endif
	\end
	ret

.if \overlap == 1
	/*
	 * Copy backwards.
	 */
        ALIGN_TEXT
2:
	addq	%rcx,%rdi
	addq	%rcx,%rsi

	cmpq	$32,%rcx
	jb	2016f

	cmpq	$256,%rcx
	ja	2256f

2032:
	movq	-8(%rsi),%rdx
	movq	%rdx,-8(%rdi)
	movq	-16(%rsi),%rdx
	movq	%rdx,-16(%rdi)
	movq	-24(%rsi),%rdx
	movq	%rdx,-24(%rdi)
	movq	-32(%rsi),%rdx
	movq	%rdx,-32(%rdi)
	leaq	-32(%rsi),%rsi
	leaq	-32(%rdi),%rdi
	subq	$32,%rcx
	cmpq	$32,%rcx
	jae	2032b
	cmpb	$0,%cl
	jne	2016f
	\end
	ret
	ALIGN_TEXT
2016:
	cmpb	$16,%cl
	jl	2008f
	movq	-8(%rsi),%rdx
	movq	%rdx,-8(%rdi)
	movq	-16(%rsi),%rdx
	movq	%rdx,-16(%rdi)
	subb	$16,%cl
	jz	2000f
	leaq	-16(%rsi),%rsi
	leaq	-16(%rdi),%rdi
2008:
	cmpb	$8,%cl
	jl	2004f
	movq	-8(%rsi),%rdx
	movq	%rdx,-8(%rdi)
	subb	$8,%cl
	jz	2000f
	leaq	-8(%rsi),%rsi
	leaq	-8(%rdi),%rdi
2004:
	cmpb	$4,%cl
	jl	2002f
	movl	-4(%rsi),%edx
	movl	%edx,-4(%rdi)
	subb	$4,%cl
	jz	2000f
	leaq	-4(%rsi),%rsi
	leaq	-4(%rdi),%rdi
2002:
	cmpb	$2,%cl
	jl	2001f
	movw	-2(%rsi),%dx
	movw	%dx,-2(%rdi)
	subb	$2,%cl
	jz	2000f
	leaq	-2(%rsi),%rsi
	leaq	-2(%rdi),%rdi
2001:
	cmpb	$1,%cl
	jl	2000f
	movb	-1(%rsi),%dl
	movb	%dl,-1(%rdi)
2000:
	\end
	ret
	ALIGN_TEXT
2256:
	decq	%rdi
	decq	%rsi
	std
.if \erms == 1
	rep
	movsb
.else
	andq	$7,%rcx                         /* any fractional bytes? */
	je	3f
	rep
	movsb
3:
	movq	%rdx,%rcx                       /* copy remainder by 32-bit words */
	shrq	$3,%rcx
	subq	$7,%rsi
	subq	$7,%rdi
	rep
	movsq
.endif
	cld
	\end
	ret
.endif
.endm

.macro MEMMOVE_BEGIN
	movq	%rdi,%rax
	movq	%rdx,%rcx
.endm

.macro MEMMOVE_END
.endm

#ifndef MEMCPY
ENTRY(memmove)
	MEMMOVE erms=0 overlap=1 begin=MEMMOVE_BEGIN end=MEMMOVE_END
END(memmove)
#else
ENTRY(memcpy)
	MEMMOVE erms=0 overlap=1 begin=MEMMOVE_BEGIN end=MEMMOVE_END
END(memcpy)
#endif
