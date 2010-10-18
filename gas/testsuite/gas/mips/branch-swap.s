	.set push
	.set mips2
1:	beqzl	$2, 1b
	b	1b
foo:	beqzl	$2, foo
	b	foo

	.set pop
	.space 8
