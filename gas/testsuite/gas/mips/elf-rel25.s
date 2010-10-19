	.abicalls
	.globl	foo
	.ent	foo
foo:
	.set	noreorder
	.cpload $25
	.set	reorder
	.end	foo
