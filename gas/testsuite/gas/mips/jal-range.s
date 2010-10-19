# Source file used to test misaligned targets of absolute jumps

	jal	0x0
	jal	0x1
	jal	0xffffffc
	jal	0xfffffff
	jal	0x10000000
	jal	0x10000003
