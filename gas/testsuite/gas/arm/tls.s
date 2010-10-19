	.text
	.globl main
	.type main, %function
main:
	nop
.L2:
	nop
	mov	pc, lr

.Lpool:
	.word	a(tlsgd) + (. - .L2 - 8)
	.word	b(tlsldm) + (. - .L2 - 8)
	.word	c(gottpoff) + (. - .L2 - 8)
	.word	d(tpoff)
