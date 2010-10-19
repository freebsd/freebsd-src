	@ Test that macro expansions are properly scrubbed.
	.macro popret regs
	ldmia sp!, {\regs, pc}
	.endm
	.text
l:
	popret "r4, r5"

	@ section padding for a.out's sake
	nop
	nop
	nop
