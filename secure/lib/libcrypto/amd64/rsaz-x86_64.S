/* $FreeBSD$ */
/* Do not modify. This file is auto-generated from rsaz-x86_64.pl. */
.text	



.globl	rsaz_512_sqr
.type	rsaz_512_sqr,@function
.align	32
rsaz_512_sqr:
.cfi_startproc	
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

	subq	$128+24,%rsp
.cfi_adjust_cfa_offset	128+24
.Lsqr_body:
.byte	102,72,15,110,202
	movq	(%rsi),%rdx
	movq	8(%rsi),%rax
	movq	%rcx,128(%rsp)
	jmp	.Loop_sqr

.align	32
.Loop_sqr:
	movl	%r8d,128+8(%rsp)

	movq	%rdx,%rbx
	movq	%rax,%rbp
	mulq	%rdx
	movq	%rax,%r8
	movq	16(%rsi),%rax
	movq	%rdx,%r9

	mulq	%rbx
	addq	%rax,%r9
	movq	24(%rsi),%rax
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r10
	movq	32(%rsi),%rax
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r11
	movq	40(%rsi),%rax
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r12
	movq	48(%rsi),%rax
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r13
	movq	56(%rsi),%rax
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r14
	movq	%rbx,%rax
	adcq	$0,%rdx

	xorq	%rcx,%rcx
	addq	%r8,%r8
	movq	%rdx,%r15
	adcq	$0,%rcx

	mulq	%rax
	addq	%r8,%rdx
	adcq	$0,%rcx

	movq	%rax,(%rsp)
	movq	%rdx,8(%rsp)


	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	movq	24(%rsi),%rax
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r11
	movq	32(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r11
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r12
	movq	40(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r12
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r13
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r13
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r14
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r14
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r15
	movq	%rbp,%rax
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx

	xorq	%rbx,%rbx
	addq	%r9,%r9
	movq	%rdx,%r8
	adcq	%r10,%r10
	adcq	$0,%rbx

	mulq	%rax

	addq	%rcx,%rax
	movq	16(%rsi),%rbp
	addq	%rax,%r9
	movq	24(%rsi),%rax
	adcq	%rdx,%r10
	adcq	$0,%rbx

	movq	%r9,16(%rsp)
	movq	%r10,24(%rsp)


	mulq	%rbp
	addq	%rax,%r12
	movq	32(%rsi),%rax
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%rbp
	addq	%rax,%r13
	movq	40(%rsi),%rax
	adcq	$0,%rdx
	addq	%rcx,%r13
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%rbp
	addq	%rax,%r14
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%rcx,%r14
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%rbp
	addq	%rax,%r15
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%rcx,%r15
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%rbp
	addq	%rax,%r8
	movq	%rbp,%rax
	adcq	$0,%rdx
	addq	%rcx,%r8
	adcq	$0,%rdx

	xorq	%rcx,%rcx
	addq	%r11,%r11
	movq	%rdx,%r9
	adcq	%r12,%r12
	adcq	$0,%rcx

	mulq	%rax

	addq	%rbx,%rax
	movq	24(%rsi),%r10
	addq	%rax,%r11
	movq	32(%rsi),%rax
	adcq	%rdx,%r12
	adcq	$0,%rcx

	movq	%r11,32(%rsp)
	movq	%r12,40(%rsp)


	movq	%rax,%r11
	mulq	%r10
	addq	%rax,%r14
	movq	40(%rsi),%rax
	movq	%rdx,%rbx
	adcq	$0,%rbx

	movq	%rax,%r12
	mulq	%r10
	addq	%rax,%r15
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r15
	movq	%rdx,%rbx
	adcq	$0,%rbx

	movq	%rax,%rbp
	mulq	%r10
	addq	%rax,%r8
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r8
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%r10
	addq	%rax,%r9
	movq	%r10,%rax
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx

	xorq	%rbx,%rbx
	addq	%r13,%r13
	movq	%rdx,%r10
	adcq	%r14,%r14
	adcq	$0,%rbx

	mulq	%rax

	addq	%rcx,%rax
	addq	%rax,%r13
	movq	%r12,%rax
	adcq	%rdx,%r14
	adcq	$0,%rbx

	movq	%r13,48(%rsp)
	movq	%r14,56(%rsp)


	mulq	%r11
	addq	%rax,%r8
	movq	%rbp,%rax
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%r11
	addq	%rax,%r9
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%rcx,%r9
	movq	%rdx,%rcx
	adcq	$0,%rcx

	movq	%rax,%r14
	mulq	%r11
	addq	%rax,%r10
	movq	%r11,%rax
	adcq	$0,%rdx
	addq	%rcx,%r10
	adcq	$0,%rdx

	xorq	%rcx,%rcx
	addq	%r15,%r15
	movq	%rdx,%r11
	adcq	%r8,%r8
	adcq	$0,%rcx

	mulq	%rax

	addq	%rbx,%rax
	addq	%rax,%r15
	movq	%rbp,%rax
	adcq	%rdx,%r8
	adcq	$0,%rcx

	movq	%r15,64(%rsp)
	movq	%r8,72(%rsp)


	mulq	%r12
	addq	%rax,%r10
	movq	%r14,%rax
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%r12
	addq	%rax,%r11
	movq	%r12,%rax
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx

	xorq	%rbx,%rbx
	addq	%r9,%r9
	movq	%rdx,%r12
	adcq	%r10,%r10
	adcq	$0,%rbx

	mulq	%rax

	addq	%rcx,%rax
	addq	%rax,%r9
	movq	%r14,%rax
	adcq	%rdx,%r10
	adcq	$0,%rbx

	movq	%r9,80(%rsp)
	movq	%r10,88(%rsp)


	mulq	%rbp
	addq	%rax,%r12
	movq	%rbp,%rax
	adcq	$0,%rdx

	xorq	%rcx,%rcx
	addq	%r11,%r11
	movq	%rdx,%r13
	adcq	%r12,%r12
	adcq	$0,%rcx

	mulq	%rax

	addq	%rbx,%rax
	addq	%rax,%r11
	movq	%r14,%rax
	adcq	%rdx,%r12
	adcq	$0,%rcx

	movq	%r11,96(%rsp)
	movq	%r12,104(%rsp)


	xorq	%rbx,%rbx
	addq	%r13,%r13
	adcq	$0,%rbx

	mulq	%rax

	addq	%rcx,%rax
	addq	%r13,%rax
	adcq	%rbx,%rdx

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15
.byte	102,72,15,126,205

	movq	%rax,112(%rsp)
	movq	%rdx,120(%rsp)

	call	__rsaz_512_reduce

	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	movq	%r8,%rdx
	movq	%r9,%rax
	movl	128+8(%rsp),%r8d
	movq	%rdi,%rsi

	decl	%r8d
	jnz	.Loop_sqr

	leaq	128+24+48(%rsp),%rax
.cfi_def_cfa	%rax,8
	movq	-48(%rax),%r15
.cfi_restore	%r15
	movq	-40(%rax),%r14
.cfi_restore	%r14
	movq	-32(%rax),%r13
.cfi_restore	%r13
	movq	-24(%rax),%r12
.cfi_restore	%r12
	movq	-16(%rax),%rbp
.cfi_restore	%rbp
	movq	-8(%rax),%rbx
.cfi_restore	%rbx
	leaq	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lsqr_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_512_sqr,.-rsaz_512_sqr
.globl	rsaz_512_mul
.type	rsaz_512_mul,@function
.align	32
rsaz_512_mul:
.cfi_startproc	
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

	subq	$128+24,%rsp
.cfi_adjust_cfa_offset	128+24
.Lmul_body:
.byte	102,72,15,110,199
.byte	102,72,15,110,201
	movq	%r8,128(%rsp)
	movq	(%rdx),%rbx
	movq	%rdx,%rbp
	call	__rsaz_512_mul

.byte	102,72,15,126,199
.byte	102,72,15,126,205

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reduce
	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	leaq	128+24+48(%rsp),%rax
.cfi_def_cfa	%rax,8
	movq	-48(%rax),%r15
.cfi_restore	%r15
	movq	-40(%rax),%r14
.cfi_restore	%r14
	movq	-32(%rax),%r13
.cfi_restore	%r13
	movq	-24(%rax),%r12
.cfi_restore	%r12
	movq	-16(%rax),%rbp
.cfi_restore	%rbp
	movq	-8(%rax),%rbx
.cfi_restore	%rbx
	leaq	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lmul_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_512_mul,.-rsaz_512_mul
.globl	rsaz_512_mul_gather4
.type	rsaz_512_mul_gather4,@function
.align	32
rsaz_512_mul_gather4:
.cfi_startproc	
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

	subq	$152,%rsp
.cfi_adjust_cfa_offset	152
.Lmul_gather4_body:
	movd	%r9d,%xmm8
	movdqa	.Linc+16(%rip),%xmm1
	movdqa	.Linc(%rip),%xmm0

	pshufd	$0,%xmm8,%xmm8
	movdqa	%xmm1,%xmm7
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm8,%xmm0
	movdqa	%xmm7,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm8,%xmm1
	movdqa	%xmm7,%xmm4
	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm8,%xmm2
	movdqa	%xmm7,%xmm5
	paddd	%xmm3,%xmm4
	pcmpeqd	%xmm8,%xmm3
	movdqa	%xmm7,%xmm6
	paddd	%xmm4,%xmm5
	pcmpeqd	%xmm8,%xmm4
	paddd	%xmm5,%xmm6
	pcmpeqd	%xmm8,%xmm5
	paddd	%xmm6,%xmm7
	pcmpeqd	%xmm8,%xmm6
	pcmpeqd	%xmm8,%xmm7

	movdqa	0(%rdx),%xmm8
	movdqa	16(%rdx),%xmm9
	movdqa	32(%rdx),%xmm10
	movdqa	48(%rdx),%xmm11
	pand	%xmm0,%xmm8
	movdqa	64(%rdx),%xmm12
	pand	%xmm1,%xmm9
	movdqa	80(%rdx),%xmm13
	pand	%xmm2,%xmm10
	movdqa	96(%rdx),%xmm14
	pand	%xmm3,%xmm11
	movdqa	112(%rdx),%xmm15
	leaq	128(%rdx),%rbp
	pand	%xmm4,%xmm12
	pand	%xmm5,%xmm13
	pand	%xmm6,%xmm14
	pand	%xmm7,%xmm15
	por	%xmm10,%xmm8
	por	%xmm11,%xmm9
	por	%xmm12,%xmm8
	por	%xmm13,%xmm9
	por	%xmm14,%xmm8
	por	%xmm15,%xmm9

	por	%xmm9,%xmm8
	pshufd	$0x4e,%xmm8,%xmm9
	por	%xmm9,%xmm8
.byte	102,76,15,126,195

	movq	%r8,128(%rsp)
	movq	%rdi,128+8(%rsp)
	movq	%rcx,128+16(%rsp)

	movq	(%rsi),%rax
	movq	8(%rsi),%rcx
	mulq	%rbx
	movq	%rax,(%rsp)
	movq	%rcx,%rax
	movq	%rdx,%r8

	mulq	%rbx
	addq	%rax,%r8
	movq	16(%rsi),%rax
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r9
	movq	24(%rsi),%rax
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r10
	movq	32(%rsi),%rax
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r11
	movq	40(%rsi),%rax
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r12
	movq	48(%rsi),%rax
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r13
	movq	56(%rsi),%rax
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r14
	movq	(%rsi),%rax
	movq	%rdx,%r15
	adcq	$0,%r15

	leaq	8(%rsp),%rdi
	movl	$7,%ecx
	jmp	.Loop_mul_gather

.align	32
.Loop_mul_gather:
	movdqa	0(%rbp),%xmm8
	movdqa	16(%rbp),%xmm9
	movdqa	32(%rbp),%xmm10
	movdqa	48(%rbp),%xmm11
	pand	%xmm0,%xmm8
	movdqa	64(%rbp),%xmm12
	pand	%xmm1,%xmm9
	movdqa	80(%rbp),%xmm13
	pand	%xmm2,%xmm10
	movdqa	96(%rbp),%xmm14
	pand	%xmm3,%xmm11
	movdqa	112(%rbp),%xmm15
	leaq	128(%rbp),%rbp
	pand	%xmm4,%xmm12
	pand	%xmm5,%xmm13
	pand	%xmm6,%xmm14
	pand	%xmm7,%xmm15
	por	%xmm10,%xmm8
	por	%xmm11,%xmm9
	por	%xmm12,%xmm8
	por	%xmm13,%xmm9
	por	%xmm14,%xmm8
	por	%xmm15,%xmm9

	por	%xmm9,%xmm8
	pshufd	$0x4e,%xmm8,%xmm9
	por	%xmm9,%xmm8
.byte	102,76,15,126,195

	mulq	%rbx
	addq	%rax,%r8
	movq	8(%rsi),%rax
	movq	%r8,(%rdi)
	movq	%rdx,%r8
	adcq	$0,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r9,%r8
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r10
	movq	24(%rsi),%rax
	adcq	$0,%rdx
	addq	%r10,%r9
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r11
	movq	32(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r12
	movq	40(%rsi),%rax
	adcq	$0,%rdx
	addq	%r12,%r11
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r13
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%r13,%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r14
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%r14,%r13
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r15
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r15,%r14
	movq	%rdx,%r15
	adcq	$0,%r15

	leaq	8(%rdi),%rdi

	decl	%ecx
	jnz	.Loop_mul_gather

	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	movq	128+8(%rsp),%rdi
	movq	128+16(%rsp),%rbp

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reduce
	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	leaq	128+24+48(%rsp),%rax
.cfi_def_cfa	%rax,8
	movq	-48(%rax),%r15
.cfi_restore	%r15
	movq	-40(%rax),%r14
.cfi_restore	%r14
	movq	-32(%rax),%r13
.cfi_restore	%r13
	movq	-24(%rax),%r12
.cfi_restore	%r12
	movq	-16(%rax),%rbp
.cfi_restore	%rbp
	movq	-8(%rax),%rbx
.cfi_restore	%rbx
	leaq	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lmul_gather4_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_512_mul_gather4,.-rsaz_512_mul_gather4
.globl	rsaz_512_mul_scatter4
.type	rsaz_512_mul_scatter4,@function
.align	32
rsaz_512_mul_scatter4:
.cfi_startproc	
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

	movl	%r9d,%r9d
	subq	$128+24,%rsp
.cfi_adjust_cfa_offset	128+24
.Lmul_scatter4_body:
	leaq	(%r8,%r9,8),%r8
.byte	102,72,15,110,199
.byte	102,72,15,110,202
.byte	102,73,15,110,208
	movq	%rcx,128(%rsp)

	movq	%rdi,%rbp
	movq	(%rdi),%rbx
	call	__rsaz_512_mul

.byte	102,72,15,126,199
.byte	102,72,15,126,205

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reduce
	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
.byte	102,72,15,126,214
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	movq	%r8,0(%rsi)
	movq	%r9,128(%rsi)
	movq	%r10,256(%rsi)
	movq	%r11,384(%rsi)
	movq	%r12,512(%rsi)
	movq	%r13,640(%rsi)
	movq	%r14,768(%rsi)
	movq	%r15,896(%rsi)

	leaq	128+24+48(%rsp),%rax
.cfi_def_cfa	%rax,8
	movq	-48(%rax),%r15
.cfi_restore	%r15
	movq	-40(%rax),%r14
.cfi_restore	%r14
	movq	-32(%rax),%r13
.cfi_restore	%r13
	movq	-24(%rax),%r12
.cfi_restore	%r12
	movq	-16(%rax),%rbp
.cfi_restore	%rbp
	movq	-8(%rax),%rbx
.cfi_restore	%rbx
	leaq	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lmul_scatter4_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_512_mul_scatter4,.-rsaz_512_mul_scatter4
.globl	rsaz_512_mul_by_one
.type	rsaz_512_mul_by_one,@function
.align	32
rsaz_512_mul_by_one:
.cfi_startproc	
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

	subq	$128+24,%rsp
.cfi_adjust_cfa_offset	128+24
.Lmul_by_one_body:
	movq	%rdx,%rbp
	movq	%rcx,128(%rsp)

	movq	(%rsi),%r8
	pxor	%xmm0,%xmm0
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11
	movq	32(%rsi),%r12
	movq	40(%rsi),%r13
	movq	48(%rsi),%r14
	movq	56(%rsi),%r15

	movdqa	%xmm0,(%rsp)
	movdqa	%xmm0,16(%rsp)
	movdqa	%xmm0,32(%rsp)
	movdqa	%xmm0,48(%rsp)
	movdqa	%xmm0,64(%rsp)
	movdqa	%xmm0,80(%rsp)
	movdqa	%xmm0,96(%rsp)
	call	__rsaz_512_reduce
	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	leaq	128+24+48(%rsp),%rax
.cfi_def_cfa	%rax,8
	movq	-48(%rax),%r15
.cfi_restore	%r15
	movq	-40(%rax),%r14
.cfi_restore	%r14
	movq	-32(%rax),%r13
.cfi_restore	%r13
	movq	-24(%rax),%r12
.cfi_restore	%r12
	movq	-16(%rax),%rbp
.cfi_restore	%rbp
	movq	-8(%rax),%rbx
.cfi_restore	%rbx
	leaq	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lmul_by_one_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_512_mul_by_one,.-rsaz_512_mul_by_one
.type	__rsaz_512_reduce,@function
.align	32
__rsaz_512_reduce:
.cfi_startproc	
	movq	%r8,%rbx
	imulq	128+8(%rsp),%rbx
	movq	0(%rbp),%rax
	movl	$8,%ecx
	jmp	.Lreduction_loop

.align	32
.Lreduction_loop:
	mulq	%rbx
	movq	8(%rbp),%rax
	negq	%r8
	movq	%rdx,%r8
	adcq	$0,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	16(%rbp),%rax
	adcq	$0,%rdx
	addq	%r9,%r8
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r10
	movq	24(%rbp),%rax
	adcq	$0,%rdx
	addq	%r10,%r9
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r11
	movq	32(%rbp),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	128+8(%rsp),%rsi


	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbx
	addq	%rax,%r12
	movq	40(%rbp),%rax
	adcq	$0,%rdx
	imulq	%r8,%rsi
	addq	%r12,%r11
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r13
	movq	48(%rbp),%rax
	adcq	$0,%rdx
	addq	%r13,%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r14
	movq	56(%rbp),%rax
	adcq	$0,%rdx
	addq	%r14,%r13
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	movq	%rsi,%rbx
	addq	%rax,%r15
	movq	0(%rbp),%rax
	adcq	$0,%rdx
	addq	%r15,%r14
	movq	%rdx,%r15
	adcq	$0,%r15

	decl	%ecx
	jne	.Lreduction_loop

	.byte	0xf3,0xc3
.cfi_endproc	
.size	__rsaz_512_reduce,.-__rsaz_512_reduce
.type	__rsaz_512_subtract,@function
.align	32
__rsaz_512_subtract:
.cfi_startproc	
	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	movq	0(%rbp),%r8
	movq	8(%rbp),%r9
	negq	%r8
	notq	%r9
	andq	%rcx,%r8
	movq	16(%rbp),%r10
	andq	%rcx,%r9
	notq	%r10
	movq	24(%rbp),%r11
	andq	%rcx,%r10
	notq	%r11
	movq	32(%rbp),%r12
	andq	%rcx,%r11
	notq	%r12
	movq	40(%rbp),%r13
	andq	%rcx,%r12
	notq	%r13
	movq	48(%rbp),%r14
	andq	%rcx,%r13
	notq	%r14
	movq	56(%rbp),%r15
	andq	%rcx,%r14
	notq	%r15
	andq	%rcx,%r15

	addq	(%rdi),%r8
	adcq	8(%rdi),%r9
	adcq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15

	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	.byte	0xf3,0xc3
.cfi_endproc	
.size	__rsaz_512_subtract,.-__rsaz_512_subtract
.type	__rsaz_512_mul,@function
.align	32
__rsaz_512_mul:
.cfi_startproc	
	leaq	8(%rsp),%rdi

	movq	(%rsi),%rax
	mulq	%rbx
	movq	%rax,(%rdi)
	movq	8(%rsi),%rax
	movq	%rdx,%r8

	mulq	%rbx
	addq	%rax,%r8
	movq	16(%rsi),%rax
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r9
	movq	24(%rsi),%rax
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r10
	movq	32(%rsi),%rax
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r11
	movq	40(%rsi),%rax
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r12
	movq	48(%rsi),%rax
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r13
	movq	56(%rsi),%rax
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r14
	movq	(%rsi),%rax
	movq	%rdx,%r15
	adcq	$0,%r15

	leaq	8(%rbp),%rbp
	leaq	8(%rdi),%rdi

	movl	$7,%ecx
	jmp	.Loop_mul

.align	32
.Loop_mul:
	movq	(%rbp),%rbx
	mulq	%rbx
	addq	%rax,%r8
	movq	8(%rsi),%rax
	movq	%r8,(%rdi)
	movq	%rdx,%r8
	adcq	$0,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r9,%r8
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r10
	movq	24(%rsi),%rax
	adcq	$0,%rdx
	addq	%r10,%r9
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r11
	movq	32(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r12
	movq	40(%rsi),%rax
	adcq	$0,%rdx
	addq	%r12,%r11
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r13
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%r13,%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r14
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%r14,%r13
	movq	%rdx,%r14
	leaq	8(%rbp),%rbp
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r15
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r15,%r14
	movq	%rdx,%r15
	adcq	$0,%r15

	leaq	8(%rdi),%rdi

	decl	%ecx
	jnz	.Loop_mul

	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	.byte	0xf3,0xc3
.cfi_endproc	
.size	__rsaz_512_mul,.-__rsaz_512_mul
.globl	rsaz_512_scatter4
.type	rsaz_512_scatter4,@function
.align	16
rsaz_512_scatter4:
.cfi_startproc	
	leaq	(%rdi,%rdx,8),%rdi
	movl	$8,%r9d
	jmp	.Loop_scatter
.align	16
.Loop_scatter:
	movq	(%rsi),%rax
	leaq	8(%rsi),%rsi
	movq	%rax,(%rdi)
	leaq	128(%rdi),%rdi
	decl	%r9d
	jnz	.Loop_scatter
	.byte	0xf3,0xc3
.cfi_endproc	
.size	rsaz_512_scatter4,.-rsaz_512_scatter4

.globl	rsaz_512_gather4
.type	rsaz_512_gather4,@function
.align	16
rsaz_512_gather4:
.cfi_startproc	
	movd	%edx,%xmm8
	movdqa	.Linc+16(%rip),%xmm1
	movdqa	.Linc(%rip),%xmm0

	pshufd	$0,%xmm8,%xmm8
	movdqa	%xmm1,%xmm7
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm8,%xmm0
	movdqa	%xmm7,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm8,%xmm1
	movdqa	%xmm7,%xmm4
	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm8,%xmm2
	movdqa	%xmm7,%xmm5
	paddd	%xmm3,%xmm4
	pcmpeqd	%xmm8,%xmm3
	movdqa	%xmm7,%xmm6
	paddd	%xmm4,%xmm5
	pcmpeqd	%xmm8,%xmm4
	paddd	%xmm5,%xmm6
	pcmpeqd	%xmm8,%xmm5
	paddd	%xmm6,%xmm7
	pcmpeqd	%xmm8,%xmm6
	pcmpeqd	%xmm8,%xmm7
	movl	$8,%r9d
	jmp	.Loop_gather
.align	16
.Loop_gather:
	movdqa	0(%rsi),%xmm8
	movdqa	16(%rsi),%xmm9
	movdqa	32(%rsi),%xmm10
	movdqa	48(%rsi),%xmm11
	pand	%xmm0,%xmm8
	movdqa	64(%rsi),%xmm12
	pand	%xmm1,%xmm9
	movdqa	80(%rsi),%xmm13
	pand	%xmm2,%xmm10
	movdqa	96(%rsi),%xmm14
	pand	%xmm3,%xmm11
	movdqa	112(%rsi),%xmm15
	leaq	128(%rsi),%rsi
	pand	%xmm4,%xmm12
	pand	%xmm5,%xmm13
	pand	%xmm6,%xmm14
	pand	%xmm7,%xmm15
	por	%xmm10,%xmm8
	por	%xmm11,%xmm9
	por	%xmm12,%xmm8
	por	%xmm13,%xmm9
	por	%xmm14,%xmm8
	por	%xmm15,%xmm9

	por	%xmm9,%xmm8
	pshufd	$0x4e,%xmm8,%xmm9
	por	%xmm9,%xmm8
	movq	%xmm8,(%rdi)
	leaq	8(%rdi),%rdi
	decl	%r9d
	jnz	.Loop_gather
	.byte	0xf3,0xc3
.LSEH_end_rsaz_512_gather4:
.cfi_endproc	
.size	rsaz_512_gather4,.-rsaz_512_gather4

.align	64
.Linc:
.long	0,0, 1,1
.long	2,2, 2,2
