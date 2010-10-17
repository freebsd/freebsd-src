
	.globl	foo
	.ent	foo
foo:
	nop
	la	$2, bar - foo
	.end	foo

	.p2align 5
