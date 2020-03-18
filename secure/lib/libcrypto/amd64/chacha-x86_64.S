/* $FreeBSD$ */
/* Do not modify. This file is auto-generated from chacha-x86_64.pl. */
.text	



.align	64
.Lzero:
.long	0,0,0,0
.Lone:
.long	1,0,0,0
.Linc:
.long	0,1,2,3
.Lfour:
.long	4,4,4,4
.Lincy:
.long	0,2,4,6,1,3,5,7
.Leight:
.long	8,8,8,8,8,8,8,8
.Lrot16:
.byte	0x2,0x3,0x0,0x1, 0x6,0x7,0x4,0x5, 0xa,0xb,0x8,0x9, 0xe,0xf,0xc,0xd
.Lrot24:
.byte	0x3,0x0,0x1,0x2, 0x7,0x4,0x5,0x6, 0xb,0x8,0x9,0xa, 0xf,0xc,0xd,0xe
.Ltwoy:
.long	2,0,0,0, 2,0,0,0
.align	64
.Lzeroz:
.long	0,0,0,0, 1,0,0,0, 2,0,0,0, 3,0,0,0
.Lfourz:
.long	4,0,0,0, 4,0,0,0, 4,0,0,0, 4,0,0,0
.Lincz:
.long	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
.Lsixteen:
.long	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16
.Lsigma:
.byte	101,120,112,97,110,100,32,51,50,45,98,121,116,101,32,107,0
.byte	67,104,97,67,104,97,50,48,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.globl	ChaCha20_ctr32
.type	ChaCha20_ctr32,@function
.align	64
ChaCha20_ctr32:
.cfi_startproc	
	cmpq	$0,%rdx
	je	.Lno_data
	movq	OPENSSL_ia32cap_P+4(%rip),%r10
	testl	$512,%r10d
	jnz	.LChaCha20_ssse3

	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-16
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-24
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_adjust_cfa_offset	8
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_adjust_cfa_offset	8
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_adjust_cfa_offset	8
.cfi_offset	%r15,-56
	subq	$64+24,%rsp
.cfi_adjust_cfa_offset	64+24
.Lctr32_body:


	movdqu	(%rcx),%xmm1
	movdqu	16(%rcx),%xmm2
	movdqu	(%r8),%xmm3
	movdqa	.Lone(%rip),%xmm4


	movdqa	%xmm1,16(%rsp)
	movdqa	%xmm2,32(%rsp)
	movdqa	%xmm3,48(%rsp)
	movq	%rdx,%rbp
	jmp	.Loop_outer

.align	32
.Loop_outer:
	movl	$0x61707865,%eax
	movl	$0x3320646e,%ebx
	movl	$0x79622d32,%ecx
	movl	$0x6b206574,%edx
	movl	16(%rsp),%r8d
	movl	20(%rsp),%r9d
	movl	24(%rsp),%r10d
	movl	28(%rsp),%r11d
	movd	%xmm3,%r12d
	movl	52(%rsp),%r13d
	movl	56(%rsp),%r14d
	movl	60(%rsp),%r15d

	movq	%rbp,64+0(%rsp)
	movl	$10,%ebp
	movq	%rsi,64+8(%rsp)
.byte	102,72,15,126,214
	movq	%rdi,64+16(%rsp)
	movq	%rsi,%rdi
	shrq	$32,%rdi
	jmp	.Loop

.align	32
.Loop:
	addl	%r8d,%eax
	xorl	%eax,%r12d
	roll	$16,%r12d
	addl	%r9d,%ebx
	xorl	%ebx,%r13d
	roll	$16,%r13d
	addl	%r12d,%esi
	xorl	%esi,%r8d
	roll	$12,%r8d
	addl	%r13d,%edi
	xorl	%edi,%r9d
	roll	$12,%r9d
	addl	%r8d,%eax
	xorl	%eax,%r12d
	roll	$8,%r12d
	addl	%r9d,%ebx
	xorl	%ebx,%r13d
	roll	$8,%r13d
	addl	%r12d,%esi
	xorl	%esi,%r8d
	roll	$7,%r8d
	addl	%r13d,%edi
	xorl	%edi,%r9d
	roll	$7,%r9d
	movl	%esi,32(%rsp)
	movl	%edi,36(%rsp)
	movl	40(%rsp),%esi
	movl	44(%rsp),%edi
	addl	%r10d,%ecx
	xorl	%ecx,%r14d
	roll	$16,%r14d
	addl	%r11d,%edx
	xorl	%edx,%r15d
	roll	$16,%r15d
	addl	%r14d,%esi
	xorl	%esi,%r10d
	roll	$12,%r10d
	addl	%r15d,%edi
	xorl	%edi,%r11d
	roll	$12,%r11d
	addl	%r10d,%ecx
	xorl	%ecx,%r14d
	roll	$8,%r14d
	addl	%r11d,%edx
	xorl	%edx,%r15d
	roll	$8,%r15d
	addl	%r14d,%esi
	xorl	%esi,%r10d
	roll	$7,%r10d
	addl	%r15d,%edi
	xorl	%edi,%r11d
	roll	$7,%r11d
	addl	%r9d,%eax
	xorl	%eax,%r15d
	roll	$16,%r15d
	addl	%r10d,%ebx
	xorl	%ebx,%r12d
	roll	$16,%r12d
	addl	%r15d,%esi
	xorl	%esi,%r9d
	roll	$12,%r9d
	addl	%r12d,%edi
	xorl	%edi,%r10d
	roll	$12,%r10d
	addl	%r9d,%eax
	xorl	%eax,%r15d
	roll	$8,%r15d
	addl	%r10d,%ebx
	xorl	%ebx,%r12d
	roll	$8,%r12d
	addl	%r15d,%esi
	xorl	%esi,%r9d
	roll	$7,%r9d
	addl	%r12d,%edi
	xorl	%edi,%r10d
	roll	$7,%r10d
	movl	%esi,40(%rsp)
	movl	%edi,44(%rsp)
	movl	32(%rsp),%esi
	movl	36(%rsp),%edi
	addl	%r11d,%ecx
	xorl	%ecx,%r13d
	roll	$16,%r13d
	addl	%r8d,%edx
	xorl	%edx,%r14d
	roll	$16,%r14d
	addl	%r13d,%esi
	xorl	%esi,%r11d
	roll	$12,%r11d
	addl	%r14d,%edi
	xorl	%edi,%r8d
	roll	$12,%r8d
	addl	%r11d,%ecx
	xorl	%ecx,%r13d
	roll	$8,%r13d
	addl	%r8d,%edx
	xorl	%edx,%r14d
	roll	$8,%r14d
	addl	%r13d,%esi
	xorl	%esi,%r11d
	roll	$7,%r11d
	addl	%r14d,%edi
	xorl	%edi,%r8d
	roll	$7,%r8d
	decl	%ebp
	jnz	.Loop
	movl	%edi,36(%rsp)
	movl	%esi,32(%rsp)
	movq	64(%rsp),%rbp
	movdqa	%xmm2,%xmm1
	movq	64+8(%rsp),%rsi
	paddd	%xmm4,%xmm3
	movq	64+16(%rsp),%rdi

	addl	$0x61707865,%eax
	addl	$0x3320646e,%ebx
	addl	$0x79622d32,%ecx
	addl	$0x6b206574,%edx
	addl	16(%rsp),%r8d
	addl	20(%rsp),%r9d
	addl	24(%rsp),%r10d
	addl	28(%rsp),%r11d
	addl	48(%rsp),%r12d
	addl	52(%rsp),%r13d
	addl	56(%rsp),%r14d
	addl	60(%rsp),%r15d
	paddd	32(%rsp),%xmm1

	cmpq	$64,%rbp
	jb	.Ltail

	xorl	0(%rsi),%eax
	xorl	4(%rsi),%ebx
	xorl	8(%rsi),%ecx
	xorl	12(%rsi),%edx
	xorl	16(%rsi),%r8d
	xorl	20(%rsi),%r9d
	xorl	24(%rsi),%r10d
	xorl	28(%rsi),%r11d
	movdqu	32(%rsi),%xmm0
	xorl	48(%rsi),%r12d
	xorl	52(%rsi),%r13d
	xorl	56(%rsi),%r14d
	xorl	60(%rsi),%r15d
	leaq	64(%rsi),%rsi
	pxor	%xmm1,%xmm0

	movdqa	%xmm2,32(%rsp)
	movd	%xmm3,48(%rsp)

	movl	%eax,0(%rdi)
	movl	%ebx,4(%rdi)
	movl	%ecx,8(%rdi)
	movl	%edx,12(%rdi)
	movl	%r8d,16(%rdi)
	movl	%r9d,20(%rdi)
	movl	%r10d,24(%rdi)
	movl	%r11d,28(%rdi)
	movdqu	%xmm0,32(%rdi)
	movl	%r12d,48(%rdi)
	movl	%r13d,52(%rdi)
	movl	%r14d,56(%rdi)
	movl	%r15d,60(%rdi)
	leaq	64(%rdi),%rdi

	subq	$64,%rbp
	jnz	.Loop_outer

	jmp	.Ldone

.align	16
.Ltail:
	movl	%eax,0(%rsp)
	movl	%ebx,4(%rsp)
	xorq	%rbx,%rbx
	movl	%ecx,8(%rsp)
	movl	%edx,12(%rsp)
	movl	%r8d,16(%rsp)
	movl	%r9d,20(%rsp)
	movl	%r10d,24(%rsp)
	movl	%r11d,28(%rsp)
	movdqa	%xmm1,32(%rsp)
	movl	%r12d,48(%rsp)
	movl	%r13d,52(%rsp)
	movl	%r14d,56(%rsp)
	movl	%r15d,60(%rsp)

.Loop_tail:
	movzbl	(%rsi,%rbx,1),%eax
	movzbl	(%rsp,%rbx,1),%edx
	leaq	1(%rbx),%rbx
	xorl	%edx,%eax
	movb	%al,-1(%rdi,%rbx,1)
	decq	%rbp
	jnz	.Loop_tail

.Ldone:
	leaq	64+24+48(%rsp),%rsi
.cfi_def_cfa	%rsi,8
	movq	-48(%rsi),%r15
.cfi_restore	%r15
	movq	-40(%rsi),%r14
.cfi_restore	%r14
	movq	-32(%rsi),%r13
.cfi_restore	%r13
	movq	-24(%rsi),%r12
.cfi_restore	%r12
	movq	-16(%rsi),%rbp
.cfi_restore	%rbp
	movq	-8(%rsi),%rbx
.cfi_restore	%rbx
	leaq	(%rsi),%rsp
.cfi_def_cfa_register	%rsp
.Lno_data:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ChaCha20_ctr32,.-ChaCha20_ctr32
.type	ChaCha20_ssse3,@function
.align	32
ChaCha20_ssse3:
.cfi_startproc	
.LChaCha20_ssse3:
	movq	%rsp,%r9
.cfi_def_cfa_register	%r9
	cmpq	$128,%rdx
	je	.LChaCha20_128
	ja	.LChaCha20_4x

.Ldo_sse3_after_all:
	subq	$64+8,%rsp
	movdqa	.Lsigma(%rip),%xmm0
	movdqu	(%rcx),%xmm1
	movdqu	16(%rcx),%xmm2
	movdqu	(%r8),%xmm3
	movdqa	.Lrot16(%rip),%xmm6
	movdqa	.Lrot24(%rip),%xmm7

	movdqa	%xmm0,0(%rsp)
	movdqa	%xmm1,16(%rsp)
	movdqa	%xmm2,32(%rsp)
	movdqa	%xmm3,48(%rsp)
	movq	$10,%r8
	jmp	.Loop_ssse3

.align	32
.Loop_outer_ssse3:
	movdqa	.Lone(%rip),%xmm3
	movdqa	0(%rsp),%xmm0
	movdqa	16(%rsp),%xmm1
	movdqa	32(%rsp),%xmm2
	paddd	48(%rsp),%xmm3
	movq	$10,%r8
	movdqa	%xmm3,48(%rsp)
	jmp	.Loop_ssse3

.align	32
.Loop_ssse3:
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,222
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$20,%xmm1
	pslld	$12,%xmm4
	por	%xmm4,%xmm1
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,223
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$25,%xmm1
	pslld	$7,%xmm4
	por	%xmm4,%xmm1
	pshufd	$78,%xmm2,%xmm2
	pshufd	$57,%xmm1,%xmm1
	pshufd	$147,%xmm3,%xmm3
	nop
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,222
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$20,%xmm1
	pslld	$12,%xmm4
	por	%xmm4,%xmm1
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,223
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$25,%xmm1
	pslld	$7,%xmm4
	por	%xmm4,%xmm1
	pshufd	$78,%xmm2,%xmm2
	pshufd	$147,%xmm1,%xmm1
	pshufd	$57,%xmm3,%xmm3
	decq	%r8
	jnz	.Loop_ssse3
	paddd	0(%rsp),%xmm0
	paddd	16(%rsp),%xmm1
	paddd	32(%rsp),%xmm2
	paddd	48(%rsp),%xmm3

	cmpq	$64,%rdx
	jb	.Ltail_ssse3

	movdqu	0(%rsi),%xmm4
	movdqu	16(%rsi),%xmm5
	pxor	%xmm4,%xmm0
	movdqu	32(%rsi),%xmm4
	pxor	%xmm5,%xmm1
	movdqu	48(%rsi),%xmm5
	leaq	64(%rsi),%rsi
	pxor	%xmm4,%xmm2
	pxor	%xmm5,%xmm3

	movdqu	%xmm0,0(%rdi)
	movdqu	%xmm1,16(%rdi)
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm3,48(%rdi)
	leaq	64(%rdi),%rdi

	subq	$64,%rdx
	jnz	.Loop_outer_ssse3

	jmp	.Ldone_ssse3

.align	16
.Ltail_ssse3:
	movdqa	%xmm0,0(%rsp)
	movdqa	%xmm1,16(%rsp)
	movdqa	%xmm2,32(%rsp)
	movdqa	%xmm3,48(%rsp)
	xorq	%r8,%r8

.Loop_tail_ssse3:
	movzbl	(%rsi,%r8,1),%eax
	movzbl	(%rsp,%r8,1),%ecx
	leaq	1(%r8),%r8
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r8,1)
	decq	%rdx
	jnz	.Loop_tail_ssse3

.Ldone_ssse3:
	leaq	(%r9),%rsp
.cfi_def_cfa_register	%rsp
.Lssse3_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ChaCha20_ssse3,.-ChaCha20_ssse3
.type	ChaCha20_128,@function
.align	32
ChaCha20_128:
.cfi_startproc	
.LChaCha20_128:
	movq	%rsp,%r9
.cfi_def_cfa_register	%r9
	subq	$64+8,%rsp
	movdqa	.Lsigma(%rip),%xmm8
	movdqu	(%rcx),%xmm9
	movdqu	16(%rcx),%xmm2
	movdqu	(%r8),%xmm3
	movdqa	.Lone(%rip),%xmm1
	movdqa	.Lrot16(%rip),%xmm6
	movdqa	.Lrot24(%rip),%xmm7

	movdqa	%xmm8,%xmm10
	movdqa	%xmm8,0(%rsp)
	movdqa	%xmm9,%xmm11
	movdqa	%xmm9,16(%rsp)
	movdqa	%xmm2,%xmm0
	movdqa	%xmm2,32(%rsp)
	paddd	%xmm3,%xmm1
	movdqa	%xmm3,48(%rsp)
	movq	$10,%r8
	jmp	.Loop_128

.align	32
.Loop_128:
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
.byte	102,15,56,0,222
.byte	102,15,56,0,206
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$20,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$12,%xmm4
	psrld	$20,%xmm11
	por	%xmm4,%xmm9
	pslld	$12,%xmm5
	por	%xmm5,%xmm11
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
.byte	102,15,56,0,223
.byte	102,15,56,0,207
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$25,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$7,%xmm4
	psrld	$25,%xmm11
	por	%xmm4,%xmm9
	pslld	$7,%xmm5
	por	%xmm5,%xmm11
	pshufd	$78,%xmm2,%xmm2
	pshufd	$57,%xmm9,%xmm9
	pshufd	$147,%xmm3,%xmm3
	pshufd	$78,%xmm0,%xmm0
	pshufd	$57,%xmm11,%xmm11
	pshufd	$147,%xmm1,%xmm1
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
.byte	102,15,56,0,222
.byte	102,15,56,0,206
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$20,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$12,%xmm4
	psrld	$20,%xmm11
	por	%xmm4,%xmm9
	pslld	$12,%xmm5
	por	%xmm5,%xmm11
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
.byte	102,15,56,0,223
.byte	102,15,56,0,207
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$25,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$7,%xmm4
	psrld	$25,%xmm11
	por	%xmm4,%xmm9
	pslld	$7,%xmm5
	por	%xmm5,%xmm11
	pshufd	$78,%xmm2,%xmm2
	pshufd	$147,%xmm9,%xmm9
	pshufd	$57,%xmm3,%xmm3
	pshufd	$78,%xmm0,%xmm0
	pshufd	$147,%xmm11,%xmm11
	pshufd	$57,%xmm1,%xmm1
	decq	%r8
	jnz	.Loop_128
	paddd	0(%rsp),%xmm8
	paddd	16(%rsp),%xmm9
	paddd	32(%rsp),%xmm2
	paddd	48(%rsp),%xmm3
	paddd	.Lone(%rip),%xmm1
	paddd	0(%rsp),%xmm10
	paddd	16(%rsp),%xmm11
	paddd	32(%rsp),%xmm0
	paddd	48(%rsp),%xmm1

	movdqu	0(%rsi),%xmm4
	movdqu	16(%rsi),%xmm5
	pxor	%xmm4,%xmm8
	movdqu	32(%rsi),%xmm4
	pxor	%xmm5,%xmm9
	movdqu	48(%rsi),%xmm5
	pxor	%xmm4,%xmm2
	movdqu	64(%rsi),%xmm4
	pxor	%xmm5,%xmm3
	movdqu	80(%rsi),%xmm5
	pxor	%xmm4,%xmm10
	movdqu	96(%rsi),%xmm4
	pxor	%xmm5,%xmm11
	movdqu	112(%rsi),%xmm5
	pxor	%xmm4,%xmm0
	pxor	%xmm5,%xmm1

	movdqu	%xmm8,0(%rdi)
	movdqu	%xmm9,16(%rdi)
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm3,48(%rdi)
	movdqu	%xmm10,64(%rdi)
	movdqu	%xmm11,80(%rdi)
	movdqu	%xmm0,96(%rdi)
	movdqu	%xmm1,112(%rdi)
	leaq	(%r9),%rsp
.cfi_def_cfa_register	%rsp
.L128_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ChaCha20_128,.-ChaCha20_128
.type	ChaCha20_4x,@function
.align	32
ChaCha20_4x:
.cfi_startproc	
.LChaCha20_4x:
	movq	%rsp,%r9
.cfi_def_cfa_register	%r9
	movq	%r10,%r11
	cmpq	$192,%rdx
	ja	.Lproceed4x

	andq	$71303168,%r11
	cmpq	$4194304,%r11
	je	.Ldo_sse3_after_all

.Lproceed4x:
	subq	$0x140+8,%rsp
	movdqa	.Lsigma(%rip),%xmm11
	movdqu	(%rcx),%xmm15
	movdqu	16(%rcx),%xmm7
	movdqu	(%r8),%xmm3
	leaq	256(%rsp),%rcx
	leaq	.Lrot16(%rip),%r10
	leaq	.Lrot24(%rip),%r11

	pshufd	$0x00,%xmm11,%xmm8
	pshufd	$0x55,%xmm11,%xmm9
	movdqa	%xmm8,64(%rsp)
	pshufd	$0xaa,%xmm11,%xmm10
	movdqa	%xmm9,80(%rsp)
	pshufd	$0xff,%xmm11,%xmm11
	movdqa	%xmm10,96(%rsp)
	movdqa	%xmm11,112(%rsp)

	pshufd	$0x00,%xmm15,%xmm12
	pshufd	$0x55,%xmm15,%xmm13
	movdqa	%xmm12,128-256(%rcx)
	pshufd	$0xaa,%xmm15,%xmm14
	movdqa	%xmm13,144-256(%rcx)
	pshufd	$0xff,%xmm15,%xmm15
	movdqa	%xmm14,160-256(%rcx)
	movdqa	%xmm15,176-256(%rcx)

	pshufd	$0x00,%xmm7,%xmm4
	pshufd	$0x55,%xmm7,%xmm5
	movdqa	%xmm4,192-256(%rcx)
	pshufd	$0xaa,%xmm7,%xmm6
	movdqa	%xmm5,208-256(%rcx)
	pshufd	$0xff,%xmm7,%xmm7
	movdqa	%xmm6,224-256(%rcx)
	movdqa	%xmm7,240-256(%rcx)

	pshufd	$0x00,%xmm3,%xmm0
	pshufd	$0x55,%xmm3,%xmm1
	paddd	.Linc(%rip),%xmm0
	pshufd	$0xaa,%xmm3,%xmm2
	movdqa	%xmm1,272-256(%rcx)
	pshufd	$0xff,%xmm3,%xmm3
	movdqa	%xmm2,288-256(%rcx)
	movdqa	%xmm3,304-256(%rcx)

	jmp	.Loop_enter4x

.align	32
.Loop_outer4x:
	movdqa	64(%rsp),%xmm8
	movdqa	80(%rsp),%xmm9
	movdqa	96(%rsp),%xmm10
	movdqa	112(%rsp),%xmm11
	movdqa	128-256(%rcx),%xmm12
	movdqa	144-256(%rcx),%xmm13
	movdqa	160-256(%rcx),%xmm14
	movdqa	176-256(%rcx),%xmm15
	movdqa	192-256(%rcx),%xmm4
	movdqa	208-256(%rcx),%xmm5
	movdqa	224-256(%rcx),%xmm6
	movdqa	240-256(%rcx),%xmm7
	movdqa	256-256(%rcx),%xmm0
	movdqa	272-256(%rcx),%xmm1
	movdqa	288-256(%rcx),%xmm2
	movdqa	304-256(%rcx),%xmm3
	paddd	.Lfour(%rip),%xmm0

.Loop_enter4x:
	movdqa	%xmm6,32(%rsp)
	movdqa	%xmm7,48(%rsp)
	movdqa	(%r10),%xmm7
	movl	$10,%eax
	movdqa	%xmm0,256-256(%rcx)
	jmp	.Loop4x

.align	32
.Loop4x:
	paddd	%xmm12,%xmm8
	paddd	%xmm13,%xmm9
	pxor	%xmm8,%xmm0
	pxor	%xmm9,%xmm1
.byte	102,15,56,0,199
.byte	102,15,56,0,207
	paddd	%xmm0,%xmm4
	paddd	%xmm1,%xmm5
	pxor	%xmm4,%xmm12
	pxor	%xmm5,%xmm13
	movdqa	%xmm12,%xmm6
	pslld	$12,%xmm12
	psrld	$20,%xmm6
	movdqa	%xmm13,%xmm7
	pslld	$12,%xmm13
	por	%xmm6,%xmm12
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm13
	paddd	%xmm12,%xmm8
	paddd	%xmm13,%xmm9
	pxor	%xmm8,%xmm0
	pxor	%xmm9,%xmm1
.byte	102,15,56,0,198
.byte	102,15,56,0,206
	paddd	%xmm0,%xmm4
	paddd	%xmm1,%xmm5
	pxor	%xmm4,%xmm12
	pxor	%xmm5,%xmm13
	movdqa	%xmm12,%xmm7
	pslld	$7,%xmm12
	psrld	$25,%xmm7
	movdqa	%xmm13,%xmm6
	pslld	$7,%xmm13
	por	%xmm7,%xmm12
	psrld	$25,%xmm6
	movdqa	(%r10),%xmm7
	por	%xmm6,%xmm13
	movdqa	%xmm4,0(%rsp)
	movdqa	%xmm5,16(%rsp)
	movdqa	32(%rsp),%xmm4
	movdqa	48(%rsp),%xmm5
	paddd	%xmm14,%xmm10
	paddd	%xmm15,%xmm11
	pxor	%xmm10,%xmm2
	pxor	%xmm11,%xmm3
.byte	102,15,56,0,215
.byte	102,15,56,0,223
	paddd	%xmm2,%xmm4
	paddd	%xmm3,%xmm5
	pxor	%xmm4,%xmm14
	pxor	%xmm5,%xmm15
	movdqa	%xmm14,%xmm6
	pslld	$12,%xmm14
	psrld	$20,%xmm6
	movdqa	%xmm15,%xmm7
	pslld	$12,%xmm15
	por	%xmm6,%xmm14
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm15
	paddd	%xmm14,%xmm10
	paddd	%xmm15,%xmm11
	pxor	%xmm10,%xmm2
	pxor	%xmm11,%xmm3
.byte	102,15,56,0,214
.byte	102,15,56,0,222
	paddd	%xmm2,%xmm4
	paddd	%xmm3,%xmm5
	pxor	%xmm4,%xmm14
	pxor	%xmm5,%xmm15
	movdqa	%xmm14,%xmm7
	pslld	$7,%xmm14
	psrld	$25,%xmm7
	movdqa	%xmm15,%xmm6
	pslld	$7,%xmm15
	por	%xmm7,%xmm14
	psrld	$25,%xmm6
	movdqa	(%r10),%xmm7
	por	%xmm6,%xmm15
	paddd	%xmm13,%xmm8
	paddd	%xmm14,%xmm9
	pxor	%xmm8,%xmm3
	pxor	%xmm9,%xmm0
.byte	102,15,56,0,223
.byte	102,15,56,0,199
	paddd	%xmm3,%xmm4
	paddd	%xmm0,%xmm5
	pxor	%xmm4,%xmm13
	pxor	%xmm5,%xmm14
	movdqa	%xmm13,%xmm6
	pslld	$12,%xmm13
	psrld	$20,%xmm6
	movdqa	%xmm14,%xmm7
	pslld	$12,%xmm14
	por	%xmm6,%xmm13
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm14
	paddd	%xmm13,%xmm8
	paddd	%xmm14,%xmm9
	pxor	%xmm8,%xmm3
	pxor	%xmm9,%xmm0
.byte	102,15,56,0,222
.byte	102,15,56,0,198
	paddd	%xmm3,%xmm4
	paddd	%xmm0,%xmm5
	pxor	%xmm4,%xmm13
	pxor	%xmm5,%xmm14
	movdqa	%xmm13,%xmm7
	pslld	$7,%xmm13
	psrld	$25,%xmm7
	movdqa	%xmm14,%xmm6
	pslld	$7,%xmm14
	por	%xmm7,%xmm13
	psrld	$25,%xmm6
	movdqa	(%r10),%xmm7
	por	%xmm6,%xmm14
	movdqa	%xmm4,32(%rsp)
	movdqa	%xmm5,48(%rsp)
	movdqa	0(%rsp),%xmm4
	movdqa	16(%rsp),%xmm5
	paddd	%xmm15,%xmm10
	paddd	%xmm12,%xmm11
	pxor	%xmm10,%xmm1
	pxor	%xmm11,%xmm2
.byte	102,15,56,0,207
.byte	102,15,56,0,215
	paddd	%xmm1,%xmm4
	paddd	%xmm2,%xmm5
	pxor	%xmm4,%xmm15
	pxor	%xmm5,%xmm12
	movdqa	%xmm15,%xmm6
	pslld	$12,%xmm15
	psrld	$20,%xmm6
	movdqa	%xmm12,%xmm7
	pslld	$12,%xmm12
	por	%xmm6,%xmm15
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm12
	paddd	%xmm15,%xmm10
	paddd	%xmm12,%xmm11
	pxor	%xmm10,%xmm1
	pxor	%xmm11,%xmm2
.byte	102,15,56,0,206
.byte	102,15,56,0,214
	paddd	%xmm1,%xmm4
	paddd	%xmm2,%xmm5
	pxor	%xmm4,%xmm15
	pxor	%xmm5,%xmm12
	movdqa	%xmm15,%xmm7
	pslld	$7,%xmm15
	psrld	$25,%xmm7
	movdqa	%xmm12,%xmm6
	pslld	$7,%xmm12
	por	%xmm7,%xmm15
	psrld	$25,%xmm6
	movdqa	(%r10),%xmm7
	por	%xmm6,%xmm12
	decl	%eax
	jnz	.Loop4x

	paddd	64(%rsp),%xmm8
	paddd	80(%rsp),%xmm9
	paddd	96(%rsp),%xmm10
	paddd	112(%rsp),%xmm11

	movdqa	%xmm8,%xmm6
	punpckldq	%xmm9,%xmm8
	movdqa	%xmm10,%xmm7
	punpckldq	%xmm11,%xmm10
	punpckhdq	%xmm9,%xmm6
	punpckhdq	%xmm11,%xmm7
	movdqa	%xmm8,%xmm9
	punpcklqdq	%xmm10,%xmm8
	movdqa	%xmm6,%xmm11
	punpcklqdq	%xmm7,%xmm6
	punpckhqdq	%xmm10,%xmm9
	punpckhqdq	%xmm7,%xmm11
	paddd	128-256(%rcx),%xmm12
	paddd	144-256(%rcx),%xmm13
	paddd	160-256(%rcx),%xmm14
	paddd	176-256(%rcx),%xmm15

	movdqa	%xmm8,0(%rsp)
	movdqa	%xmm9,16(%rsp)
	movdqa	32(%rsp),%xmm8
	movdqa	48(%rsp),%xmm9

	movdqa	%xmm12,%xmm10
	punpckldq	%xmm13,%xmm12
	movdqa	%xmm14,%xmm7
	punpckldq	%xmm15,%xmm14
	punpckhdq	%xmm13,%xmm10
	punpckhdq	%xmm15,%xmm7
	movdqa	%xmm12,%xmm13
	punpcklqdq	%xmm14,%xmm12
	movdqa	%xmm10,%xmm15
	punpcklqdq	%xmm7,%xmm10
	punpckhqdq	%xmm14,%xmm13
	punpckhqdq	%xmm7,%xmm15
	paddd	192-256(%rcx),%xmm4
	paddd	208-256(%rcx),%xmm5
	paddd	224-256(%rcx),%xmm8
	paddd	240-256(%rcx),%xmm9

	movdqa	%xmm6,32(%rsp)
	movdqa	%xmm11,48(%rsp)

	movdqa	%xmm4,%xmm14
	punpckldq	%xmm5,%xmm4
	movdqa	%xmm8,%xmm7
	punpckldq	%xmm9,%xmm8
	punpckhdq	%xmm5,%xmm14
	punpckhdq	%xmm9,%xmm7
	movdqa	%xmm4,%xmm5
	punpcklqdq	%xmm8,%xmm4
	movdqa	%xmm14,%xmm9
	punpcklqdq	%xmm7,%xmm14
	punpckhqdq	%xmm8,%xmm5
	punpckhqdq	%xmm7,%xmm9
	paddd	256-256(%rcx),%xmm0
	paddd	272-256(%rcx),%xmm1
	paddd	288-256(%rcx),%xmm2
	paddd	304-256(%rcx),%xmm3

	movdqa	%xmm0,%xmm8
	punpckldq	%xmm1,%xmm0
	movdqa	%xmm2,%xmm7
	punpckldq	%xmm3,%xmm2
	punpckhdq	%xmm1,%xmm8
	punpckhdq	%xmm3,%xmm7
	movdqa	%xmm0,%xmm1
	punpcklqdq	%xmm2,%xmm0
	movdqa	%xmm8,%xmm3
	punpcklqdq	%xmm7,%xmm8
	punpckhqdq	%xmm2,%xmm1
	punpckhqdq	%xmm7,%xmm3
	cmpq	$256,%rdx
	jb	.Ltail4x

	movdqu	0(%rsi),%xmm6
	movdqu	16(%rsi),%xmm11
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm7
	pxor	0(%rsp),%xmm6
	pxor	%xmm12,%xmm11
	pxor	%xmm4,%xmm2
	pxor	%xmm0,%xmm7

	movdqu	%xmm6,0(%rdi)
	movdqu	64(%rsi),%xmm6
	movdqu	%xmm11,16(%rdi)
	movdqu	80(%rsi),%xmm11
	movdqu	%xmm2,32(%rdi)
	movdqu	96(%rsi),%xmm2
	movdqu	%xmm7,48(%rdi)
	movdqu	112(%rsi),%xmm7
	leaq	128(%rsi),%rsi
	pxor	16(%rsp),%xmm6
	pxor	%xmm13,%xmm11
	pxor	%xmm5,%xmm2
	pxor	%xmm1,%xmm7

	movdqu	%xmm6,64(%rdi)
	movdqu	0(%rsi),%xmm6
	movdqu	%xmm11,80(%rdi)
	movdqu	16(%rsi),%xmm11
	movdqu	%xmm2,96(%rdi)
	movdqu	32(%rsi),%xmm2
	movdqu	%xmm7,112(%rdi)
	leaq	128(%rdi),%rdi
	movdqu	48(%rsi),%xmm7
	pxor	32(%rsp),%xmm6
	pxor	%xmm10,%xmm11
	pxor	%xmm14,%xmm2
	pxor	%xmm8,%xmm7

	movdqu	%xmm6,0(%rdi)
	movdqu	64(%rsi),%xmm6
	movdqu	%xmm11,16(%rdi)
	movdqu	80(%rsi),%xmm11
	movdqu	%xmm2,32(%rdi)
	movdqu	96(%rsi),%xmm2
	movdqu	%xmm7,48(%rdi)
	movdqu	112(%rsi),%xmm7
	leaq	128(%rsi),%rsi
	pxor	48(%rsp),%xmm6
	pxor	%xmm15,%xmm11
	pxor	%xmm9,%xmm2
	pxor	%xmm3,%xmm7
	movdqu	%xmm6,64(%rdi)
	movdqu	%xmm11,80(%rdi)
	movdqu	%xmm2,96(%rdi)
	movdqu	%xmm7,112(%rdi)
	leaq	128(%rdi),%rdi

	subq	$256,%rdx
	jnz	.Loop_outer4x

	jmp	.Ldone4x

.Ltail4x:
	cmpq	$192,%rdx
	jae	.L192_or_more4x
	cmpq	$128,%rdx
	jae	.L128_or_more4x
	cmpq	$64,%rdx
	jae	.L64_or_more4x


	xorq	%r10,%r10

	movdqa	%xmm12,16(%rsp)
	movdqa	%xmm4,32(%rsp)
	movdqa	%xmm0,48(%rsp)
	jmp	.Loop_tail4x

.align	32
.L64_or_more4x:
	movdqu	0(%rsi),%xmm6
	movdqu	16(%rsi),%xmm11
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm7
	pxor	0(%rsp),%xmm6
	pxor	%xmm12,%xmm11
	pxor	%xmm4,%xmm2
	pxor	%xmm0,%xmm7
	movdqu	%xmm6,0(%rdi)
	movdqu	%xmm11,16(%rdi)
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm7,48(%rdi)
	je	.Ldone4x

	movdqa	16(%rsp),%xmm6
	leaq	64(%rsi),%rsi
	xorq	%r10,%r10
	movdqa	%xmm6,0(%rsp)
	movdqa	%xmm13,16(%rsp)
	leaq	64(%rdi),%rdi
	movdqa	%xmm5,32(%rsp)
	subq	$64,%rdx
	movdqa	%xmm1,48(%rsp)
	jmp	.Loop_tail4x

.align	32
.L128_or_more4x:
	movdqu	0(%rsi),%xmm6
	movdqu	16(%rsi),%xmm11
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm7
	pxor	0(%rsp),%xmm6
	pxor	%xmm12,%xmm11
	pxor	%xmm4,%xmm2
	pxor	%xmm0,%xmm7

	movdqu	%xmm6,0(%rdi)
	movdqu	64(%rsi),%xmm6
	movdqu	%xmm11,16(%rdi)
	movdqu	80(%rsi),%xmm11
	movdqu	%xmm2,32(%rdi)
	movdqu	96(%rsi),%xmm2
	movdqu	%xmm7,48(%rdi)
	movdqu	112(%rsi),%xmm7
	pxor	16(%rsp),%xmm6
	pxor	%xmm13,%xmm11
	pxor	%xmm5,%xmm2
	pxor	%xmm1,%xmm7
	movdqu	%xmm6,64(%rdi)
	movdqu	%xmm11,80(%rdi)
	movdqu	%xmm2,96(%rdi)
	movdqu	%xmm7,112(%rdi)
	je	.Ldone4x

	movdqa	32(%rsp),%xmm6
	leaq	128(%rsi),%rsi
	xorq	%r10,%r10
	movdqa	%xmm6,0(%rsp)
	movdqa	%xmm10,16(%rsp)
	leaq	128(%rdi),%rdi
	movdqa	%xmm14,32(%rsp)
	subq	$128,%rdx
	movdqa	%xmm8,48(%rsp)
	jmp	.Loop_tail4x

.align	32
.L192_or_more4x:
	movdqu	0(%rsi),%xmm6
	movdqu	16(%rsi),%xmm11
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm7
	pxor	0(%rsp),%xmm6
	pxor	%xmm12,%xmm11
	pxor	%xmm4,%xmm2
	pxor	%xmm0,%xmm7

	movdqu	%xmm6,0(%rdi)
	movdqu	64(%rsi),%xmm6
	movdqu	%xmm11,16(%rdi)
	movdqu	80(%rsi),%xmm11
	movdqu	%xmm2,32(%rdi)
	movdqu	96(%rsi),%xmm2
	movdqu	%xmm7,48(%rdi)
	movdqu	112(%rsi),%xmm7
	leaq	128(%rsi),%rsi
	pxor	16(%rsp),%xmm6
	pxor	%xmm13,%xmm11
	pxor	%xmm5,%xmm2
	pxor	%xmm1,%xmm7

	movdqu	%xmm6,64(%rdi)
	movdqu	0(%rsi),%xmm6
	movdqu	%xmm11,80(%rdi)
	movdqu	16(%rsi),%xmm11
	movdqu	%xmm2,96(%rdi)
	movdqu	32(%rsi),%xmm2
	movdqu	%xmm7,112(%rdi)
	leaq	128(%rdi),%rdi
	movdqu	48(%rsi),%xmm7
	pxor	32(%rsp),%xmm6
	pxor	%xmm10,%xmm11
	pxor	%xmm14,%xmm2
	pxor	%xmm8,%xmm7
	movdqu	%xmm6,0(%rdi)
	movdqu	%xmm11,16(%rdi)
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm7,48(%rdi)
	je	.Ldone4x

	movdqa	48(%rsp),%xmm6
	leaq	64(%rsi),%rsi
	xorq	%r10,%r10
	movdqa	%xmm6,0(%rsp)
	movdqa	%xmm15,16(%rsp)
	leaq	64(%rdi),%rdi
	movdqa	%xmm9,32(%rsp)
	subq	$192,%rdx
	movdqa	%xmm3,48(%rsp)

.Loop_tail4x:
	movzbl	(%rsi,%r10,1),%eax
	movzbl	(%rsp,%r10,1),%ecx
	leaq	1(%r10),%r10
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r10,1)
	decq	%rdx
	jnz	.Loop_tail4x

.Ldone4x:
	leaq	(%r9),%rsp
.cfi_def_cfa_register	%rsp
.L4x_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ChaCha20_4x,.-ChaCha20_4x
