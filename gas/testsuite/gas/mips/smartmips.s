# Source file used to test SmartMIPS instruction set
	
	.text
stuff:
	ror	$4,$5,$6
	rorv	$4,$5,$6
	rotr	$4,$5,$6
	rotrv	$4,$5,$6
	
	ror	$4,$5,31
	ror	$4,$5,8
	ror	$4,$5,1
	ror	$4,$5,0
	rotr	$4,$5,31
	
	rol	$4,$5,31
	rol	$4,$5,8
	rol	$4,$5,1
	rol	$4,$5,0
	
	lwxs	$2,$4($5)
	
	maddp	$16,$17
	multp	$11,$12
	
	mflhxu	$8
	mtlhx	$4
	
	pperm	$6,$24

	.p2align 4
