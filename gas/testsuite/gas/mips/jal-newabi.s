# Source file used to test the jal macro for NewABI.
label:
	dli $4, 0x12345678
	jal label

# Make objdump print ...
	.space 8
