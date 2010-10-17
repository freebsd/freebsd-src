	.global __tls_get_addr,gd,ld
	.type __tls_get_addr,@function

	.section ".tbss","awT",@nobits
	.align 2
gd:	.space 4

	.section ".tdata","awT",@progbits
	.align 2
ld:	.long 0xc0ffee

	.text
__tls_get_addr:
	blr
