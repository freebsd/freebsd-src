# jal relocs against undefined weak symbols should not be treated as
# overflowing

	.globl	start
	.weak	foo
start:
	jal	foo
