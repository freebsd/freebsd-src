/*-
 * Copyright (c) 2023, The FreeBSD Foundation
 *
 * SPDX-License-Expression: BSD-2-Clause
 *
 * Portions of this software were developed by Robert Clausecker
 * <fuz@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 *
 * Adapted from NetBSD's common/lib/libc/arch/x86_64/string/strcpy.S
 * written by J.T. Conklin <jtc@acorntoolworks.com> and
 * adapted by Guillaume Morin <guillaume@morinfr.org> to implement stpcpy
 * that was originally dedicated to the public domain
 */

#include <machine/asm.h>

#include "amd64_archlevel.h"

#define ALIGN_TEXT	.p2align 4, 0x90

	.weak stpcpy
	.set stpcpy, __stpcpy
ARCHFUNCS(__stpcpy)
	ARCHFUNC(__stpcpy, scalar)
	ARCHFUNC(__stpcpy, baseline)
ENDARCHFUNCS(__stpcpy)

/*
 * This stpcpy implementation copies a byte at a time until the
 * source pointer is aligned to a word boundary, it then copies by
 * words until it finds a word containing a zero byte, and finally
 * copies by bytes until the end of the string is reached.
 *
 * While this may result in unaligned stores if the source and
 * destination pointers are unaligned with respect to each other,
 * it is still faster than either byte copies or the overhead of
 * an implementation suitable for machines with strict alignment
 * requirements.
 */

ARCHENTRY(__stpcpy, scalar)
	movabsq $0x0101010101010101,%r8
	movabsq $0x8080808080808080,%r9

	/*
	 * Align source to a word boundary.
	 * Consider unrolling loop?
	 */
.Lalign:
	testb	$7,%sil
	je	.Lword_aligned
	movb	(%rsi),%dl
	incq	%rsi
	movb	%dl,(%rdi)
	incq	%rdi
	testb	%dl,%dl
	jne	.Lalign
	movq	%rdi,%rax
	dec	%rax
	ret

	ALIGN_TEXT
.Lloop:
	movq	%rdx,(%rdi)
	addq	$8,%rdi
.Lword_aligned:
	movq	(%rsi),%rdx
	movq	%rdx,%rcx
	addq	$8,%rsi
	subq	%r8,%rcx
	testq	%r9,%rcx
	je	.Lloop

	/*
	 * In rare cases, the above loop may exit prematurely. We must
	 * return to the loop if none of the bytes in the word equal 0.
	 */

	movb	%dl,(%rdi)
	testb	%dl,%dl		/* 1st byte == 0? */
	je	.Ldone
	incq	%rdi

	shrq	$8,%rdx
	movb	%dl,(%rdi)
	testb	%dl,%dl		/* 2nd byte == 0? */
	je	.Ldone
	incq	%rdi

	shrq	$8,%rdx
	movb	%dl,(%rdi)
	testb	%dl,%dl		/* 3rd byte == 0? */
	je	.Ldone
	incq	%rdi

	shrq	$8,%rdx
	movb	%dl,(%rdi)
	testb	%dl,%dl		/* 4th byte == 0? */
	je	.Ldone
	incq	%rdi

	shrq	$8,%rdx
	movb	%dl,(%rdi)
	testb	%dl,%dl		/* 5th byte == 0? */
	je	.Ldone
	incq	%rdi

	shrq	$8,%rdx
	movb	%dl,(%rdi)
	testb	%dl,%dl		/* 6th byte == 0? */
	je	.Ldone
	incq	%rdi

	shrq	$8,%rdx
	movb	%dl,(%rdi)
	testb	%dl,%dl		/* 7th byte == 0? */
	je	.Ldone
	incq	%rdi

	shrq	$8,%rdx
	movb	%dl,(%rdi)
	incq	%rdi
	testb	%dl,%dl		/* 8th byte == 0? */
	jne	.Lword_aligned
	decq	%rdi

.Ldone:
	movq	%rdi,%rax
	ret
ARCHEND(__stpcpy, scalar)

ARCHENTRY(__stpcpy, baseline)
	mov	%esi, %ecx
	mov	%rdi, %rdx
	sub	%rsi, %rdi		# express destination as distance to surce
	and	$~0xf, %rsi		# align source to 16 byte
	movdqa	(%rsi), %xmm0		# head of string with junk before
	pxor	%xmm1, %xmm1
	and	$0xf, %ecx		# misalignment in bytes
	pcmpeqb	%xmm1, %xmm0		# NUL byte present?
	pmovmskb %xmm0, %eax
	shr	%cl, %eax		# clear out matches in junk bytes
	bsf	%eax, %eax		# find match if any
	jnz	.Lrunt

	/* first normal iteration: write head back if it succeeds */
	movdqa	16(%rsi), %xmm0		# 16 bytes of current iteration
	movdqu	(%rsi, %rcx, 1), %xmm2	# first 16 bytes of the string
	pcmpeqb	%xmm0, %xmm1		# NUL byte present?
	pmovmskb %xmm1, %eax
	test	%eax, %eax		# find match if any
	jnz	.Lshorty

	movdqu	%xmm2, (%rdx)		# store beginning of string

	/* main loop, unrolled twice */
	ALIGN_TEXT
0:	movdqa	32(%rsi), %xmm2		# load current iteraion
	movdqu	%xmm0, 16(%rsi, %rdi, 1) # write back previous iteraion
	pxor	%xmm1, %xmm1
	add	$32, %rsi
	pcmpeqb	%xmm2, %xmm1		# NUL byte present?
	pmovmskb %xmm1, %eax
	test	%eax, %eax
	jnz	1f

	movdqa	16(%rsi), %xmm0		# load current iteraion
	movdqu	%xmm2, (%rsi, %rdi, 1)	# write back previous iteraion
	pxor	%xmm1, %xmm1
	pcmpeqb	%xmm0, %xmm1		# NUL byte present?
	pmovmskb %xmm1, %eax
	test	%eax, %eax
	jz	0b

	/* end of string after main loop has iterated */
	add	$16, %rsi		# advance rsi to second unrolled half
1:	tzcnt	%eax, %eax		# find location of match
					# (behaves as bsf on pre-x86-64-v3 CPUs)
	add	%rsi, %rax		# point to NUL byte
	movdqu	-15(%rax), %xmm0	# last 16 bytes of string
	movdqu	%xmm0, -15(%rax, %rdi, 1) # copied to destination
	add	%rdi, %rax		# point to destination's NUL byte
	ret

	/* NUL encountered in second iteration */
.Lshorty:
	tzcnt	%eax, %eax
	add	$16, %eax		# account for length of first iteration
	sub	%ecx, %eax		# but not the parts before the string

	/* NUL encountered in first iteration */
.Lrunt:	lea	1(%rax), %edi		# string length including NUL byte
	add	%rcx, %rsi		# point to beginning of string
	add	%rdx, %rax		# point to NUL byte

	/* transfer 16--32 bytes */
.L1632:	cmp	$16, %edi
	jb	.L0815

	movdqu	-16(%rsi, %rdi, 1), %xmm0 # load last 16 bytes
	movdqu	%xmm2, (%rdx)		# store first 16 bytes
	movdqu	%xmm0, -15(%rax)	# store last 16 bytes
	ret

	/* transfer 8--15 bytes */
.L0815:	cmp	$8, %edi
	jb	.L0407

	mov	(%rsi), %rcx		# load first 8 bytes
	mov	-8(%rsi, %rdi, 1), %rdi	# load last 8 bytes
	mov	%rcx, (%rdx)		# store to dst
	mov	%rdi, -7(%rax)		# dito
	ret

	/* transfer 4--7 bytes */
.L0407:	cmp	$4, %edi
	jb	.L0203

	mov	(%rsi), %ecx
	mov	-4(%rsi, %rdi, 1), %edi
	mov	%ecx, (%rdx)
	mov	%edi, -3(%rax)
	ret

	/* transfer 2--3 bytes */
.L0203:	cmp	$2, %edi
	jb	.L0101

	movzwl	(%rsi), %ecx
	mov	%cx, (%rdx)		# store first two bytes

	/* transfer 0 bytes (last byte is always NUL) */
.L0101:	movb	$0, (%rax)		# store terminating NUL byte
	ret
ARCHEND(__stpcpy, baseline)

	.section .note.GNU-stack,"",%progbits
