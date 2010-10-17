# Source file used to test the rol and ror macros.

	# generate warnings for all uses of AT.
	.set noat

foo:
	rol	$4,$5
	rol	$4,$5,$6
	rol	$4,1
	rol	$4,$5,1
	rol	$4,$5,0

	ror	$4,$5
	ror	$4,$5,$6
	ror	$4,1
	ror	$4,$5,1
	ror	$4,$5,0

	rol	$4,$5,32
	rol	$4,$5,33
	rol	$4,$5,63

	ror	$4,$5,32
	ror	$4,$5,33
	ror	$4,$5,63

	.space	8
