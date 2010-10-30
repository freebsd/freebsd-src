	# Check that IT is accepted in ARM mode on older architectures
	.text
	.syntax unified
	.arch armv4
label1:
	it	eq
	moveq	r0, #0
	mov	pc, lr
