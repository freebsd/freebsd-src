	.text
	.align 0
label:	
	bxj     r0
	bxj	r1
	bxj	r14
	bxjeq	r0
	bxjmi	r0
	bxjpl	r7

	bkpt		@ Support for a breakpoint without an argument
	bkpt	10	@ is a feature added to GAS.
