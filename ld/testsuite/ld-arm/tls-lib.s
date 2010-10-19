	.text
	.globl foo
	.type foo, %function
foo:
	nop
.L2:
	nop
	mov	pc, lr

.Lpool:
	.word	lib_gd(tlsgd) + (. - .L2 - 8)
	.word	lib_ld(tlsldm) + (. - .L2 - 8)
	.word	lib_ld(tlsldo)

	.section .tdata,"awT"
	.global lib_gd
lib_gd:
	.space	4

	.global lib_ld
lib_ld:
	.space	4
