	.section	.tbss,"awT",@nobits
	.align 4
	.skip	24
	.type	a#,@object
	.size	a#,4
a:
	data4	0
	.text
	.globl	_start#
	.proc	_start#
_start:
	.endp	_start#
	.section	.debug_foobar
	data8	@dtprel(a#)
