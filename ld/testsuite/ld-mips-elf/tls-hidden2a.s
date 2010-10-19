	.text
	lw	$2,%gottprel(tls_hidden)($28)

	.section .tdata,"awT"
	.globl	tls_hidden
	.hidden	tls_hidden
	.type	tls_hidden,@object
	.size	tls_hidden,4
	.space	0xba8
tls_hidden:
	.word	1
