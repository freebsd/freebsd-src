	.equiv early, 4

_start:
	calll	*early(%eax)
	calll	*late(%eax)
	calll	*xtrn(%eax)
	calll	*early(%bx)
	calll	*late(%bx)
	calll	*xtrn(%bx)
	movb	$early, %al
	movb	$late, %al
	movb	$xtrn, %al
	movw	$early, %ax
	movw	$late, %ax
	movw	$xtrn, %ax
	movl	$early, %eax
	movl	$late, %eax
	movl	$xtrn, %eax
	addb	$early, %al
	addb	$late, %al
	addb	$xtrn, %al
	addw	$early, %ax
	addw	$late, %ax
	addw	$xtrn, %ax
	addl	$early, %eax
	addl	$late, %eax
	addl	$xtrn, %eax
	shlb	$early, %al
	shlb	$late, %al
	shlb	$xtrn, %al
	shlw	$early, %ax
	shlw	$late, %ax
	shlw	$xtrn, %ax
	shll	$early, %eax
	shll	$late, %eax
	shll	$xtrn, %eax
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
