	.section	.tbss,"awT",@nobits
	.align 4
	.skip	24
	.type	a,@object
	.size	a,4
a:
	.zero	4
	.text
	.globl	_start
_start:
	.section	.debug_foobar
	.long	a@dtpoff
