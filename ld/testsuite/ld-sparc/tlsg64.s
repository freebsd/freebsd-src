	.section .tbss
	.align	4
	.word	0, 0, 0, 0, 0, 0
	.type	a,#tls_object
	.size	a,4
a:
	.word	0
	.text
	.globl	_start
_start:
	.section .debug_foobar
	.xword	%r_tls_dtpoff64(a)
