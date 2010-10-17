	.section	.tbss,"awT",@nobits
	.align 4
	.skip	24
	.type	a,@object
	.size	a,4
a:
	.long	0
	.text
	.globl	_start
	.ent	_start
_start:
	.end	_start
	.section	.debug_foobar
	.quad	a !dtprel
