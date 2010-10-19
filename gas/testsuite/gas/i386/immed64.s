	.equiv early, 4

_start:
	callq	*early(%rax)
	callq	*late(%rax)
	callq	*xtrn(%rax)
	callq	*early(%eax)
	callq	*late(%eax)
	callq	*xtrn(%eax)
	movb	$early, %al
	movb	$late, %al
	movb	$xtrn, %al
	movw	$early, %ax
	movw	$late, %ax
	movw	$xtrn, %ax
	movl	$early, %eax
	movl	$late, %eax
	movl	$xtrn, %eax
	movabsq	$early, %rax
	movabsq	$late, %rax
	movabsq	$xtrn, %rax
	addb	$early, %al
	addb	$late, %al
	addb	$xtrn, %al
	addw	$early, %ax
	addw	$late, %ax
	addw	$xtrn, %ax
	addl	$early, %eax
	addl	$late, %eax
	addl	$xtrn, %eax
	addq	$early, %rax
	addq	$late, %rax
	addq	$xtrn, %rax
	shlb	$early, %al
	shlb	$late, %al
	shlb	$xtrn, %al
	shlw	$early, %ax
	shlw	$late, %ax
	shlw	$xtrn, %ax
	shll	$early, %eax
	shll	$late, %eax
	shll	$xtrn, %eax
	shlq	$early, %rax
	shlq	$late, %rax
	shlq	$xtrn, %rax
	inb	$early, %al
	inb	$late, %al
	inb	$xtrn, %al
	inw	$early, %ax
	inw	$late, %ax
	inw	$xtrn, %ax
	inl	$early, %eax
	inl	$late, %eax
	inl	$xtrn, %eax

	.equiv late, 8
