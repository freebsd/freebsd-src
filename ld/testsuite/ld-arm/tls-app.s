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
	.word	app_gd(tlsgd) + (. - .L2 - 8)
	.word	app_ld(tlsldm) + (. - .L2 - 8)
	.word	app_ld(tlsldo)
	.word	app_ie(gottpoff) + (. - .L2 - 8)
	.word	app_le(tpoff)

	.section .tdata,"awT"
	.global app_gd
app_gd:
	.space	4

	.global app_ld
app_ld:
	.space	4

	.section .tbss,"awT",%nobits
	.global app_ie
app_ie:
	.space	4

	.global app_le
app_le:
	.space	4
