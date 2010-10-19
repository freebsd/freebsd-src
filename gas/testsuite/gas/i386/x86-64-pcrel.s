	.text
_start:
	movb	$(xtrn - .), %al
	movw	$(xtrn - .), %ax
	movl	$(xtrn - .), %eax
	movq	$(xtrn - .), %rax
	movabsq	$(xtrn - .), %rax

	movb	$xtrn, %al
	movw	$xtrn, %ax
	movl	$xtrn, %eax
	movq	$xtrn, %rax
	movabsq	$xtrn, %rax

	.p2align 4,0
