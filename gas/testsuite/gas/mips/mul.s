# Source file used to test the mul macro.
	
foo:
	mul	$4,$5
	mul	$4,$5,$6
	mul	$4,$5,0
	mul	$4,$5,1
	mul	$4,$5,0x8000
	mul	$4,$5,-0x8000
	mul	$4,$5,0x10000
	mul	$4,$5,0x1a5a5

# mulo and mulou are only supported for register arguments	
	mulo	$4,$5
	mulo	$4,$5,$6

	mulou	$4,$5
	mulou	$4,$5,$6

# Sanity check the 64 bit versions.
	.set	mips3
	dmul	$4,$5,$6
	dmul	$4,$5,1
	dmulo	$4,$5,$6
	dmulou	$4,$5,$6

        .space	8
