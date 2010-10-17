# Source file used to some bad data declarations.

	.globl x

	.data
foo:
	# no way these are going to hold the pointers.
	.byte	x
	.byte	x+1
