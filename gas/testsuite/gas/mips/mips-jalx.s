# Test the generation of jalx opcodes
	.set nomips16
	jalx	external_label

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4
