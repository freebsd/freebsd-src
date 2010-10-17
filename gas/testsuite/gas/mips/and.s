# Source file used to test the and macro.
	
foo:
	and	$4,$4,0
	and	$4,$4,1
	and	$4,$4,0x8000
	and	$4,$4,-0x8000
	and	$4,$4,0x10000
	and	$4,$4,0x1a5a5

# nor, or, and xor are handled by the same code.  There is a special
# case for nor, so we test all variants.
	
	nor	$4,$5,0
	nor	$4,$5,1
	nor	$4,$5,0x8000
	nor	$4,$5,-0x8000
	nor	$4,$5,0x10000
	nor	$4,$5,0x1a5a5
	
	or	$4,$5,0

	xor	$4,$5,0

	# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
	nop
