# Source file used to test the jal macro even harder
	# some space so offets won't be 0.
	.space 0xc

	.globl	g1	.text
g1:
l1:
	# some more space, so offset from label won't be 0.
	.space 0xc

	jal	g1			# 0x18
	jal	l1			# 0x20
	jal	e1			# 0x28

	j	g1			# 0x30
	j	l1			# 0x38
	j	e1			# 0x40

	jal	g1 - 0xc		# 0x48
	jal	l1 - 0xc		# 0x50
	jal	e1 - 0xc		# 0x58

	jal	g1 + 0xc		# 0x60
	jal	l1 + 0xc		# 0x68
	jal	e1 + 0xc		# 0x70

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
