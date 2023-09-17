/*
Copyright (c) 2014, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    * this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    * this list of conditions and the following disclaimer in the documentation
    * and/or other materials provided with the distribution.

    * Neither the name of Intel Corporation nor the names of its contributors
    * may be used to endorse or promote products derived from this software
    * without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef USE_AS_STRCAT

#ifndef STRLEN
# define STRLEN		strlen
#endif

#ifndef L
# define L(label)	.L##label
#endif

#ifndef cfi_startproc
# define cfi_startproc			.cfi_startproc
#endif

#ifndef cfi_endproc
# define cfi_endproc			.cfi_endproc
#endif

#ifndef ENTRY
# define ENTRY(name)			\
	.type name,  @function; 	\
	.globl name;			\
	.p2align 4;			\
name:					\
	cfi_startproc
#endif

#ifndef END
# define END(name)			\
	cfi_endproc;			\
	.size name, .-name
#endif
#define RETURN ret
	.section .text.sse2,"ax",@progbits
ENTRY (STRLEN)
/* end ifndef USE_AS_STRCAT */
#endif
	xor	%rax, %rax
	mov	%edi, %ecx
	and	$0x3f, %ecx
	pxor	%xmm0, %xmm0
	cmp	$0x30, %ecx
	ja	L(next)
	movdqu	(%rdi), %xmm1
	pcmpeqb	%xmm1, %xmm0
	pmovmskb %xmm0, %edx
	test	%edx, %edx
	jnz	L(exit_less16)
	mov	%rdi, %rax
	and	$-16, %rax
	jmp	L(align16_start)
L(next):
	mov	%rdi, %rax
	and	$-16, %rax
	pcmpeqb	(%rax), %xmm0
	mov	$-1, %r10d
	sub	%rax, %rcx
	shl	%cl, %r10d
	pmovmskb %xmm0, %edx
	and	%r10d, %edx
	jnz	L(exit)
L(align16_start):
	pxor	%xmm0, %xmm0
	pxor	%xmm1, %xmm1
	pxor	%xmm2, %xmm2
	pxor	%xmm3, %xmm3
	pcmpeqb	16(%rax), %xmm0
	pmovmskb %xmm0, %edx
	test	%edx, %edx
	jnz	L(exit16)

	pcmpeqb	32(%rax), %xmm1
	pmovmskb %xmm1, %edx
	test	%edx, %edx
	jnz	L(exit32)

	pcmpeqb	48(%rax), %xmm2
	pmovmskb %xmm2, %edx
	test	%edx, %edx
	jnz	L(exit48)

	pcmpeqb	64(%rax), %xmm3
	pmovmskb %xmm3, %edx
	test	%edx, %edx
	jnz	L(exit64)

	pcmpeqb	80(%rax), %xmm0
	add	$64, %rax
	pmovmskb %xmm0, %edx
	test	%edx, %edx
	jnz	L(exit16)

	pcmpeqb	32(%rax), %xmm1
	pmovmskb %xmm1, %edx
	test	%edx, %edx
	jnz	L(exit32)

	pcmpeqb	48(%rax), %xmm2
	pmovmskb %xmm2, %edx
	test	%edx, %edx
	jnz	L(exit48)

	pcmpeqb	64(%rax), %xmm3
	pmovmskb %xmm3, %edx
	test	%edx, %edx
	jnz	L(exit64)

	pcmpeqb	80(%rax), %xmm0
	add	$64, %rax
	pmovmskb %xmm0, %edx
	test	%edx, %edx
	jnz	L(exit16)

	pcmpeqb	32(%rax), %xmm1
	pmovmskb %xmm1, %edx
	test	%edx, %edx
	jnz	L(exit32)

	pcmpeqb	48(%rax), %xmm2
	pmovmskb %xmm2, %edx
	test	%edx, %edx
	jnz	L(exit48)

	pcmpeqb	64(%rax), %xmm3
	pmovmskb %xmm3, %edx
	test	%edx, %edx
	jnz	L(exit64)
	
	pcmpeqb	80(%rax), %xmm0
	add	$64, %rax
	pmovmskb %xmm0, %edx
	test	%edx, %edx
	jnz	L(exit16)

	pcmpeqb	32(%rax), %xmm1
	pmovmskb %xmm1, %edx
	test	%edx, %edx
	jnz	L(exit32)

	pcmpeqb	48(%rax), %xmm2
	pmovmskb %xmm2, %edx
	test	%edx, %edx
	jnz	L(exit48)

	pcmpeqb	64(%rax), %xmm3
	pmovmskb %xmm3, %edx
	test	%edx, %edx
	jnz	L(exit64)
	

	test	$0x3f, %rax
	jz	L(align64_loop)

	pcmpeqb	80(%rax), %xmm0
	add	$80, %rax
	pmovmskb %xmm0, %edx
	test	%edx, %edx
	jnz	L(exit)

	test	$0x3f, %rax
	jz	L(align64_loop)

	pcmpeqb	16(%rax), %xmm1
	add	$16, %rax
	pmovmskb %xmm1, %edx
	test	%edx, %edx
	jnz	L(exit)

	test	$0x3f, %rax
	jz	L(align64_loop)

	pcmpeqb	16(%rax), %xmm2
	add	$16, %rax
	pmovmskb %xmm2, %edx
	test	%edx, %edx
	jnz	L(exit)

	test	$0x3f, %rax
	jz	L(align64_loop)

	pcmpeqb	16(%rax), %xmm3
	add	$16, %rax
	pmovmskb %xmm3, %edx
	test	%edx, %edx
	jnz	L(exit)

	add	$16, %rax
	.p2align 4
	L(align64_loop):
	movaps	(%rax),	%xmm4
	pminub	16(%rax), 	%xmm4
	movaps	32(%rax), 	%xmm5
	pminub	48(%rax), 	%xmm5
	add	$64, 	%rax
	pminub	%xmm4,	%xmm5
	pcmpeqb	%xmm0,	%xmm5
	pmovmskb %xmm5,	%edx
	test	%edx,	%edx
	jz	L(align64_loop)


	pcmpeqb	-64(%rax), %xmm0
	sub	$80, 	%rax
	pmovmskb %xmm0, %edx
	test	%edx, %edx
	jnz	L(exit16)

	pcmpeqb	32(%rax), %xmm1
	pmovmskb %xmm1, %edx
	test	%edx, %edx
	jnz	L(exit32)

	pcmpeqb	48(%rax), %xmm2
	pmovmskb %xmm2, %edx
	test	%edx, %edx
	jnz	L(exit48)

	pcmpeqb	64(%rax), %xmm3
	pmovmskb %xmm3, %edx
	sub	%rdi, %rax
	bsf	%rdx, %rdx
	add	%rdx, %rax
	add	$64, %rax
	RETURN

	.p2align 4
L(exit):
	sub	%rdi, %rax
L(exit_less16):
	bsf	%rdx, %rdx
	add	%rdx, %rax
	RETURN
	.p2align 4
L(exit16):
	sub	%rdi, %rax
	bsf	%rdx, %rdx
	add	%rdx, %rax
	add	$16, %rax
	RETURN
	.p2align 4
L(exit32):
	sub	%rdi, %rax
	bsf	%rdx, %rdx
	add	%rdx, %rax
	add	$32, %rax
	RETURN
	.p2align 4
L(exit48):
	sub	%rdi, %rax
	bsf	%rdx, %rdx
	add	%rdx, %rax
	add	$48, %rax
	RETURN
	.p2align 4
L(exit64):
	sub	%rdi, %rax
	bsf	%rdx, %rdx
	add	%rdx, %rax
	add	$64, %rax
#ifndef USE_AS_STRCAT
	RETURN

END (STRLEN)
#endif
