# Source file used to test the jal macro even harder
	# some space so offets won't be 0.
	.space 0xc

	.globl	g1	.text
	.globl	e2	.text
g1:
l1:
	# some more space, so offset from label won't be 0.
	.space 0xc

	# Hit the case where 'value == 0' in the BFD_RELOC_16_PCREL_S2
	# handling in tc-mips.c:md_apply_fix3().
	jal	g1 - 0x20		# 0x18
	jal	l1 - 0x28		# 0x20
	jal	e1 - 0x24		# 0x28
	jal	e2 - 0x2c		# 0x30

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
