# Source file used to test the drol and dror macros.

foo:
	drol	$4,$5
	drol	$4,$5,$6
	drol	$4,1
	drol	$4,$5,0
	drol	$4,$5,1
	drol	$4,$5,31
	drol	$4,$5,32
	drol	$4,$5,33
	drol	$4,$5,63
	drol	$4,$5,64

	dror	$4,$5
	dror	$4,$5,$6
	dror	$4,1
	dror	$4,$5,0
	dror	$4,$5,1
	dror	$4,$5,31
	dror	$4,$5,32
	dror	$4,$5,33
	dror	$4,$5,63
	dror	$4,$5,64

	drol	$4,$5,65
	drol	$4,$5,95
	drol	$4,$5,96
	drol	$4,$5,97
	drol	$4,$5,127

	dror	$4,$5,65
	dror	$4,$5,95
	dror	$4,$5,96
	dror	$4,$5,97
	dror	$4,$5,127

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8
