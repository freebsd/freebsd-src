@ Test the Thumb-2 JUMP19 relocation.

	.syntax unified
	.thumb
	.global _start
_start:
	cmp	r0, r0
	beq.w   bar
	.space 65536
	.weak bar
bar:
	bx	lr
