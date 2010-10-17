# Source file used to test the li macro.

foo:
	li	$4,0
	li	$4,1
	li	$4,0x8000
	li	$4,-0x8000
	li	$4,0x10000
	li	$4,0x1a5a5

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
