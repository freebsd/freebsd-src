	.globl	start
start:
.L3:
	mov.l	.L4,r1
	.uses	.L3
	jsr	@r1
	nop
	nop
.L4:
	.long	.L5
.L5:
	nop
