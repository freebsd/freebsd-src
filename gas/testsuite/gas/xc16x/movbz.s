	.section .text
	.global _fun
xc16x_movbz:

	movbz r2,rl0
	movbz r0,0x23dd
	movbz 0x23,rl0

	