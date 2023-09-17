/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Robert Clausecker <fuz@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE
 */

#include <machine/asm.h>
#include <machine/param.h>

#include "amd64_archlevel.h"

#define ALIGN_TEXT	.p2align 4,0x90 /* 16-byte alignment, nop filled */

ARCHFUNCS(strspn)
	ARCHFUNC(strspn, scalar)
	NOARCHFUNC
	ARCHFUNC(strspn, x86_64_v2)
ENDARCHFUNCS(strspn)

ARCHENTRY(strspn, scalar)
	push	%rbp			# align stack to enable function call
	mov	%rsp, %rbp
	sub	$256, %rsp		# allocate space for lookup table

	/* check for special cases */
	movzbl	(%rsi), %edx		# first character in the set
	test	%edx, %edx
	jz	.Lzero			# empty set always returns 0

	movzbl	1(%rsi), %eax		# second character in the set
	test	%eax, %eax
	jz	.Lsingle

	/* no special case matches -- prepare lookup table */
	xor	%r8d, %r8d
	mov	$28, %ecx
0:	mov	%r8, (%rsp, %rcx, 8)
	mov	%r8, 8(%rsp, %rcx, 8)
	mov	%r8, 16(%rsp, %rcx, 8)
	mov	%r8, 24(%rsp, %rcx, 8)
	sub	$4, %ecx
	jnc	0b

	movb	$1, (%rsp, %rdx, 1)	# register first char in set
	add	$2, %rsi

	/* process remaining chars in set */
	ALIGN_TEXT
0:	movb	$1, (%rsp, %rax, 1)	# register previous char
	movzbl	(%rsi), %eax		# next char in set
	test	%eax, %eax		# end of string?
	jz	1f

	movb	$1, (%rsp, %rax, 1)
	add	$2, %rsi
	movzbl	-1(%rsi), %eax
	test	%eax, %eax
	jnz	0b

1:	mov	%rdi, %rax		# a copy of the source to iterate over

	/* find mismatch */
	ALIGN_TEXT
0:	movzbl	(%rax), %ecx
	cmpb	$0, (%rsp, %rcx, 1)
	je	2f

	movzbl	1(%rax), %ecx
	cmpb	$0, (%rsp, %rcx, 1)
	je	3f

	movzbl	2(%rax), %ecx
	cmpb	$0, (%rsp, %rcx, 1)
	je	4f

	movzbl	3(%rax), %ecx
	add	$4, %rax
	cmpb	$0, (%rsp, %rcx, 1)
	jne	0b

	sub	$3, %rax
4:	dec	%rdi
3:	inc	%rax
2:	sub	%rdi, %rax		# number of characters preceding match
	leave
	ret

	/* empty set never matches */
.Lzero:	xor	%eax, %eax
	leave
	ret

	/* find repeated single character */
	ALIGN_TEXT
.Lsingle:
	cmpb	%dl, (%rdi, %rax, 1)
	jne	1f

	cmpb	%dl, 1(%rdi, %rax, 1)
	jne	2f

	cmpb	%dl, 2(%rdi, %rax, 1)
	jne	3f

	cmpb	%dl, 3(%rdi, %rax, 1)
	lea	4(%rax), %rax
	je	.Lsingle

	sub	$3, %rax
3:	inc	%rax
2:	inc	%rax
1:	leave
	ret
ARCHEND(strspn, scalar)

	/*
	 * This kernel uses pcmpistri to do the heavy lifting.
	 * We provide three code paths, depending on set size:
	 *
	 *  0--16: one pcmpistri per 16 bytes of input
	 * 17--32: two pcmpistri per 16 bytes of input
	 *   >=33: fall back to look up table
	 */
ARCHENTRY(strspn, x86_64_v2)
	push		%rbp
	mov		%rsp, %rbp
	sub		$256, %rsp

	/* find set size and copy up to 32 bytes to (%rsp) */
	mov		%esi, %ecx
	and		$~0xf, %rsi		# align set pointer
	movdqa		(%rsi), %xmm0
	pxor		%xmm1, %xmm1
	and		$0xf, %ecx		# amount of bytes rsi is past alignment
	xor		%edx, %edx
	pcmpeqb		%xmm0, %xmm1		# end of string reached?
	movdqa		%xmm0, 32(%rsp)		# transfer head of set to stack
	pmovmskb	%xmm1, %eax
	shr		%cl, %eax		# clear out junk before string
	test		%eax, %eax		# end of set reached?
	jnz		0f

	movdqa		16(%rsi), %xmm0		# second chunk of the set
	mov		$16, %edx
	sub		%ecx, %edx		# length of set preceding xmm0
	pxor		%xmm1, %xmm1
	pcmpeqb		%xmm0, %xmm1
	movdqa		%xmm0, 48(%rsp)
	movdqu		32(%rsp, %rcx, 1), %xmm2 # head of set
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jnz		1f

	movdqa		32(%rsi), %xmm0		# third chunk
	add		$16, %edx
	pxor		%xmm1, %xmm1
	pcmpeqb		%xmm0, %xmm1
	movdqa		%xmm0, 64(%rsp)
	pmovmskb	%xmm1, %eax
	test		%eax, %eax		# still not done?
	jz		.Lgt32v2

0:	movdqu		32(%rsp, %rcx, 1), %xmm2 # head of set
1:	tzcnt		%eax, %eax
	add		%eax, %edx		# length of set (excluding NUL byte)
	cmp		$32, %edx		# above 32 bytes?
	ja		.Lgt32v2

	/*
	 * At this point we know that we want to use pcmpistri.
	 * one last problem obtains: the head of the string is not
	 * aligned and may cross a cacheline.  If this is the case,
	 * we take the part before the page boundary and repeat the
	 * last byte to fill up the xmm register.
	 */
	mov		%rdi, %rax		# save original string pointer
	lea		15(%rdi), %esi		# last byte of the head
	xor		%edi, %esi
	test		$PAGE_SIZE, %esi	# does the head cross a page?
	jz		0f

	/* head crosses page: copy to stack to fix up */
	and		$~0xf, %rax		# align head pointer temporarily
	movzbl		15(%rax), %esi		# last head byte on the page
	movdqa		(%rax), %xmm0
	movabs		$0x0101010101010101, %r8
	imul		%r8, %rsi		# repeated 8 times
	movdqa		%xmm0, (%rsp)		# head word on stack
	mov		%rsi, 16(%rsp)		# followed by filler (last byte x8)
	mov		%rsi, 24(%rsp)
	mov		%edi, %eax
	and		$0xf, %eax		# offset of head from alignment
	add		%rsp, %rax		# pointer to fake head

0:	movdqu		(%rax), %xmm1		# load head (fake or real)
	lea		16(%rdi), %rax
	and		$~0xf, %rax		# second 16 bytes of string (aligned)
1:	cmp		$16, %edx		# 16--32 bytes?
	ja		.Lgt16v2


	/* set is 2--16 bytes in size */

	/* _SIDD_UBYTE_OPS|_SIDD_CMP_EQUAL_ANY|_SIDD_LEAST_SIGNIFICANT|_SIDD_NEGATIVE_POLARITY */
	pcmpistri	$0x10, %xmm1, %xmm2	# match in head?
	jc		.Lheadmismatchv2

	ALIGN_TEXT
0:	pcmpistri	$0x10, (%rax), %xmm2
	jc		1f			# match or end of string?
	pcmpistri	$0x10, 16(%rax), %xmm2
	lea		32(%rax), %rax
	jnc		0b			# match or end of string?

	sub		$16, %rax		# go back to second half
1:	sub		%rdi, %rax		# offset of (%rax) from beginning of string
	add		%rcx, %rax		# prefix length before match/NUL
        leave
        ret

.Lheadmismatchv2:
	mov		%ecx, %eax		# prefix length before mismatch/NUL
	leave
	ret

	/* set is 17--32 bytes in size */
.Lgt16v2:
	movdqu		48(%rsp, %rcx, 1), %xmm3 # second part of set

	/* _SIDD_UBYTE_OPS|_SIDD_CMP_EQUAL_ANY|_SIDD_BIT_MASK|_SIDD_NEGATIVE_POLARITY */
	pcmpistrm	$0x10, %xmm1, %xmm2	# any mismatch in first half?
	movdqa		%xmm0, %xmm4
	pcmpistrm	$0x10, %xmm1, %xmm3	# any mismatch in the second half?
	ptest		%xmm0, %xmm4		# any entry that doesn't match either?
	jnz		2f

	ALIGN_TEXT
0:	movdqa		(%rax), %xmm1
	pcmpistrm	$0x10, %xmm1, %xmm2
	movdqa		%xmm0, %xmm4
	pcmpistrm	$0x10, %xmm1, %xmm3
	ptest		%xmm0, %xmm4
	jnz		1f
	movdqa		16(%rax), %xmm1
	add		$32, %rax
	pcmpistrm	$0x10, %xmm1, %xmm2
	movdqa		%xmm0, %xmm4
	pcmpistrm	$0x10, %xmm1, %xmm3
	ptest		%xmm0, %xmm4
	jz		0b

	sub		$16, %rax
1:	pand		%xmm4, %xmm0
	movd		%xmm0, %ecx
	sub		%rdi, %rax		# offset of %xmm1 from beginning of string
	tzcnt		%ecx, %ecx
	add		%rcx, %rax		# prefix length before match/NUL
	leave
	ret

	/* mismatch or string end in head */
2:	pand		%xmm4, %xmm0		# bit mask of mismatches (end of string counts)
	movd		%xmm0, %eax
	tzcnt		%eax, %eax		# prefix length before mismatch/NUL
	leave
	ret

	/* set is >=33 bytes in size */
.Lgt32v2:
	xorps	%xmm0, %xmm0
	mov	$256-64, %edx

	/* clear out look up table */
0:	movaps	%xmm0, (%rsp, %rdx, 1)
	movaps	%xmm0, 16(%rsp, %rdx, 1)
	movaps	%xmm0, 32(%rsp, %rdx, 1)
	movaps	%xmm0, 48(%rsp, %rdx, 1)
	sub	$64, %edx
	jnc	0b

	add	%rcx, %rsi		# restore string pointer
	mov	%rdi, %rax		# keep a copy of the string

	/* initialise look up table */
	movzbl	(%rsi), %ecx		# string is known not to be empty

	ALIGN_TEXT
0:	movb	$1, (%rsp, %rcx, 1)
	movzbl	1(%rsi), %ecx
	test	%ecx, %ecx
	jz	1f

	movb	$1, (%rsp, %rcx, 1)
	movzbl	2(%rsi), %ecx
	test	%ecx, %ecx
	jz	1f

	movb	$1, (%rsp, %rcx, 1)
	movzbl	3(%rsi), %ecx
	add	$4, %rsi
	test	%ecx, %ecx
	jz	1f

	movb	$1, (%rsp, %rcx, 1)
	movzbl	(%rsi), %ecx
	test	%ecx, %ecx
	jnz	0b

	/* find match */
	ALIGN_TEXT
1:	movzbl	(%rax), %ecx
	cmpb	$0, (%rsp, %rcx, 1)
	je	2f

	movzbl	1(%rax), %ecx
	cmpb	$0, (%rsp, %rcx, 1)
	je	3f

	movzbl	2(%rax), %ecx
	cmpb	$0, (%rsp, %rcx, 1)
	je	4f

	movzbl	3(%rax), %ecx
	add	$4, %rax
	cmpb	$0, (%rsp, %rcx, 1)
	jne	1b

	sub	$3, %rax
4:	dec	%rdi
3:	inc	%rax
2:	sub	%rdi, %rax		# number of characters preceding match
	leave
	ret
ARCHEND(strspn, x86_64_v2)

	.section .note.GNU-stack,"",%progbits
