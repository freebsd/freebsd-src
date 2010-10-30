	.weak	bar

	# This will be in low memory.
	.section .text1,"ax"
	bsr	bar
	jmp	bar

	# This will be in high memory.
	.section .text2,"ax"
	# This needs special handling
	bsr	bar
	# This shouldn't
	jmp	bar
