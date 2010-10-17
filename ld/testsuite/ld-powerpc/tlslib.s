	.global .__tls_get_addr,__tls_get_addr,gd,ld
	.type .__tls_get_addr,@function

	.section ".opd","aw",@progbits
__tls_get_addr:
	.align 3
	.quad	.__tls_get_addr
	.quad	.TOC.@tocbase
	.quad	0

	.section ".tbss","awT",@nobits
	.align 3
gd:	.space 8

	.section ".tdata","awT",@progbits
	.align 2
ld:	.long 0xc0ffee

	.text
.__tls_get_addr:
	blr
