/* $FreeBSD$ */
/* Do not modify. This file is auto-generated from poly1305-x86_64.pl. */
.text	



.globl	poly1305_init
.hidden	poly1305_init
.globl	poly1305_blocks
.hidden	poly1305_blocks
.globl	poly1305_emit
.hidden	poly1305_emit

.type	poly1305_init,@function
.align	32
poly1305_init:
.cfi_startproc	
	xorq	%rax,%rax
	movq	%rax,0(%rdi)
	movq	%rax,8(%rdi)
	movq	%rax,16(%rdi)

	cmpq	$0,%rsi
	je	.Lno_key

	leaq	poly1305_blocks(%rip),%r10
	leaq	poly1305_emit(%rip),%r11
	movq	$0x0ffffffc0fffffff,%rax
	movq	$0x0ffffffc0ffffffc,%rcx
	andq	0(%rsi),%rax
	andq	8(%rsi),%rcx
	movq	%rax,24(%rdi)
	movq	%rcx,32(%rdi)
	movq	%r10,0(%rdx)
	movq	%r11,8(%rdx)
	movl	$1,%eax
.Lno_key:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	poly1305_init,.-poly1305_init

.type	poly1305_blocks,@function
.align	32
poly1305_blocks:
.cfi_startproc	
.Lblocks:
	shrq	$4,%rdx
	jz	.Lno_data

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
.Lblocks_body:

	movq	%rdx,%r15

	movq	24(%rdi),%r11
	movq	32(%rdi),%r13

	movq	0(%rdi),%r14
	movq	8(%rdi),%rbx
	movq	16(%rdi),%rbp

	movq	%r13,%r12
	shrq	$2,%r13
	movq	%r12,%rax
	addq	%r12,%r13
	jmp	.Loop

.align	32
.Loop:
	addq	0(%rsi),%r14
	adcq	8(%rsi),%rbx
	leaq	16(%rsi),%rsi
	adcq	%rcx,%rbp
	mulq	%r14
	movq	%rax,%r9
	movq	%r11,%rax
	movq	%rdx,%r10

	mulq	%r14
	movq	%rax,%r14
	movq	%r11,%rax
	movq	%rdx,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	%r13,%rax
	adcq	%rdx,%r10

	mulq	%rbx
	movq	%rbp,%rbx
	addq	%rax,%r14
	adcq	%rdx,%r8

	imulq	%r13,%rbx
	addq	%rbx,%r9
	movq	%r8,%rbx
	adcq	$0,%r10

	imulq	%r11,%rbp
	addq	%r9,%rbx
	movq	$-4,%rax
	adcq	%rbp,%r10

	andq	%r10,%rax
	movq	%r10,%rbp
	shrq	$2,%r10
	andq	$3,%rbp
	addq	%r10,%rax
	addq	%rax,%r14
	adcq	$0,%rbx
	adcq	$0,%rbp
	movq	%r12,%rax
	decq	%r15
	jnz	.Loop

	movq	%r14,0(%rdi)
	movq	%rbx,8(%rdi)
	movq	%rbp,16(%rdi)

	movq	0(%rsp),%r15
.cfi_restore	%r15
	movq	8(%rsp),%r14
.cfi_restore	%r14
	movq	16(%rsp),%r13
.cfi_restore	%r13
	movq	24(%rsp),%r12
.cfi_restore	%r12
	movq	32(%rsp),%rbp
.cfi_restore	%rbp
	movq	40(%rsp),%rbx
.cfi_restore	%rbx
	leaq	48(%rsp),%rsp
.cfi_adjust_cfa_offset	-48
.Lno_data:
.Lblocks_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	poly1305_blocks,.-poly1305_blocks

.type	poly1305_emit,@function
.align	32
poly1305_emit:
.cfi_startproc	
.Lemit:
	movq	0(%rdi),%r8
	movq	8(%rdi),%r9
	movq	16(%rdi),%r10

	movq	%r8,%rax
	addq	$5,%r8
	movq	%r9,%rcx
	adcq	$0,%r9
	adcq	$0,%r10
	shrq	$2,%r10
	cmovnzq	%r8,%rax
	cmovnzq	%r9,%rcx

	addq	0(%rdx),%rax
	adcq	8(%rdx),%rcx
	movq	%rax,0(%rsi)
	movq	%rcx,8(%rsi)

	.byte	0xf3,0xc3
.cfi_endproc	
.size	poly1305_emit,.-poly1305_emit
.byte	80,111,108,121,49,51,48,53,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	16
.globl	xor128_encrypt_n_pad
.type	xor128_encrypt_n_pad,@function
.align	16
xor128_encrypt_n_pad:
.cfi_startproc	
	subq	%rdx,%rsi
	subq	%rdx,%rdi
	movq	%rcx,%r10
	shrq	$4,%rcx
	jz	.Ltail_enc
	nop
.Loop_enc_xmm:
	movdqu	(%rsi,%rdx,1),%xmm0
	pxor	(%rdx),%xmm0
	movdqu	%xmm0,(%rdi,%rdx,1)
	movdqa	%xmm0,(%rdx)
	leaq	16(%rdx),%rdx
	decq	%rcx
	jnz	.Loop_enc_xmm

	andq	$15,%r10
	jz	.Ldone_enc

.Ltail_enc:
	movq	$16,%rcx
	subq	%r10,%rcx
	xorl	%eax,%eax
.Loop_enc_byte:
	movb	(%rsi,%rdx,1),%al
	xorb	(%rdx),%al
	movb	%al,(%rdi,%rdx,1)
	movb	%al,(%rdx)
	leaq	1(%rdx),%rdx
	decq	%r10
	jnz	.Loop_enc_byte

	xorl	%eax,%eax
.Loop_enc_pad:
	movb	%al,(%rdx)
	leaq	1(%rdx),%rdx
	decq	%rcx
	jnz	.Loop_enc_pad

.Ldone_enc:
	movq	%rdx,%rax
	.byte	0xf3,0xc3
.cfi_endproc	
.size	xor128_encrypt_n_pad,.-xor128_encrypt_n_pad

.globl	xor128_decrypt_n_pad
.type	xor128_decrypt_n_pad,@function
.align	16
xor128_decrypt_n_pad:
.cfi_startproc	
	subq	%rdx,%rsi
	subq	%rdx,%rdi
	movq	%rcx,%r10
	shrq	$4,%rcx
	jz	.Ltail_dec
	nop
.Loop_dec_xmm:
	movdqu	(%rsi,%rdx,1),%xmm0
	movdqa	(%rdx),%xmm1
	pxor	%xmm0,%xmm1
	movdqu	%xmm1,(%rdi,%rdx,1)
	movdqa	%xmm0,(%rdx)
	leaq	16(%rdx),%rdx
	decq	%rcx
	jnz	.Loop_dec_xmm

	pxor	%xmm1,%xmm1
	andq	$15,%r10
	jz	.Ldone_dec

.Ltail_dec:
	movq	$16,%rcx
	subq	%r10,%rcx
	xorl	%eax,%eax
	xorq	%r11,%r11
.Loop_dec_byte:
	movb	(%rsi,%rdx,1),%r11b
	movb	(%rdx),%al
	xorb	%r11b,%al
	movb	%al,(%rdi,%rdx,1)
	movb	%r11b,(%rdx)
	leaq	1(%rdx),%rdx
	decq	%r10
	jnz	.Loop_dec_byte

	xorl	%eax,%eax
.Loop_dec_pad:
	movb	%al,(%rdx)
	leaq	1(%rdx),%rdx
	decq	%rcx
	jnz	.Loop_dec_pad

.Ldone_dec:
	movq	%rdx,%rax
	.byte	0xf3,0xc3
.cfi_endproc	
.size	xor128_decrypt_n_pad,.-xor128_decrypt_n_pad
