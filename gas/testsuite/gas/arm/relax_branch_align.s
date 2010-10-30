	.syntax unified
	.thumb
fn:
	nop
.L191:
	beq	.L192
.L46:
	nop
	.align	2
.L54:
	.rept 62
	.word	0
	.endr
	nop
	bne	.L46
.L192:
	nop
