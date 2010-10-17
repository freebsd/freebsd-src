	.ent	foo
foo:
	lui	$4,%hi(l2)
	la	$3,l1
	addiu	$4,$4,%lo(l2)

	.space	64
	.end	foo

	.globl	l1
	.globl	l2
	.data
l1:	.word	1
l2:	.word	2
