	.text
	.align 5
	.globl	f
f:
	mov.l	.L1,r0
	rts
	nop
	.align 2
.L1:	.long	g - f
	.long	foo@GOT

