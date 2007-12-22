	# $FreeBSD$






	.file	"rc4-586.s"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16
.globl RC4
	.type	RC4,@function
RC4:

	pushl	%ebp
	pushl	%ebx
	movl	12(%esp),	%ebp
	movl	16(%esp),	%ebx
	pushl	%esi
	pushl	%edi
	movl	(%ebp),		%ecx
	movl	4(%ebp),	%edx
	movl	28(%esp),	%esi
	incl	%ecx
	subl	$12,		%esp
	addl	$8,		%ebp
	andl	$255,		%ecx
	leal	-8(%ebx,%esi),	%ebx
	movl	44(%esp),	%edi
	movl	%ebx,		8(%esp)
	movl	(%ebp,%ecx,4),	%eax
	cmpl	%esi,		%ebx
	jl	.L000end
.L001start:
	addl	$8,		%esi

	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	%bl,		(%esp)

	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	%bl,		1(%esp)

	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	%bl,		2(%esp)

	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	%bl,		3(%esp)

	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	%bl,		4(%esp)

	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	%bl,		5(%esp)

	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	%bl,		6(%esp)

	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	addl	$8,		%edi
	movb	%bl,		7(%esp)

	movl	(%esp),		%eax
	movl	-8(%esi),	%ebx
	xorl	%ebx,		%eax
	movl	-4(%esi),	%ebx
	movl	%eax,		-8(%edi)
	movl	4(%esp),	%eax
	xorl	%ebx,		%eax
	movl	8(%esp),	%ebx
	movl	%eax,		-4(%edi)
	movl	(%ebp,%ecx,4),	%eax
	cmpl	%ebx,		%esi
	jle	.L001start
.L000end:

	addl	$8,		%ebx
	incl	%esi
	cmpl	%esi,		%ebx
	jl	.L002finished
	movl	%ebx,		8(%esp)
	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	-1(%esi),	%bh
	xorb	%bh,		%bl
	movb	%bl,		(%edi)

	movl	8(%esp),	%ebx
	cmpl	%esi,		%ebx
	jle	.L002finished
	incl	%esi
	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	-1(%esi),	%bh
	xorb	%bh,		%bl
	movb	%bl,		1(%edi)

	movl	8(%esp),	%ebx
	cmpl	%esi,		%ebx
	jle	.L002finished
	incl	%esi
	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	-1(%esi),	%bh
	xorb	%bh,		%bl
	movb	%bl,		2(%edi)

	movl	8(%esp),	%ebx
	cmpl	%esi,		%ebx
	jle	.L002finished
	incl	%esi
	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	-1(%esi),	%bh
	xorb	%bh,		%bl
	movb	%bl,		3(%edi)

	movl	8(%esp),	%ebx
	cmpl	%esi,		%ebx
	jle	.L002finished
	incl	%esi
	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	-1(%esi),	%bh
	xorb	%bh,		%bl
	movb	%bl,		4(%edi)

	movl	8(%esp),	%ebx
	cmpl	%esi,		%ebx
	jle	.L002finished
	incl	%esi
	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movl	(%ebp,%ecx,4),	%eax
	movb	-1(%esi),	%bh
	xorb	%bh,		%bl
	movb	%bl,		5(%edi)

	movl	8(%esp),	%ebx
	cmpl	%esi,		%ebx
	jle	.L002finished
	incl	%esi
	addl	%eax,		%edx
	andl	$255,		%edx
	incl	%ecx
	movl	(%ebp,%edx,4),	%ebx
	movl	%ebx,		-4(%ebp,%ecx,4)
	addl	%eax,		%ebx
	andl	$255,		%ecx
	andl	$255,		%ebx
	movl	%eax,		(%ebp,%edx,4)
	nop
	movl	(%ebp,%ebx,4),	%ebx
	movb	-1(%esi),	%bh
	xorb	%bh,		%bl
	movb	%bl,		6(%edi)
.L002finished:
	decl	%ecx
	addl	$12,		%esp
	movl	%edx,		-4(%ebp)
	movb	%cl,		-8(%ebp)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.L_RC4_end:
	.size	RC4,.L_RC4_end-RC4
.ident	"RC4"
