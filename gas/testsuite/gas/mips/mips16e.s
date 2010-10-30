# Test the mips16e instruction set.

	.set	mips16
	.text
stuff:
	# explicit compact jumps
	jalrc	$2
	jalrc	$31,$2
	jrc	$31
	jrc	$2
	
	# these jumps should all be converted to compact versions
	jalr	$2
	jalr	$31,$2
	jal	$2
	jal	$31,$2
	jr	$31
	jr	$2
	j	$31
	j	$2
	
	# make sure unconditional jumps don't swap with compact jumps
	# and vice versa.
	jalr	$2
	.set	noreorder
	jal	foo		# mustn't swap with previous jalr
	addu	$4,$2,1
	.set	reorder
	jalr	$2
	jal	foo
	
	move	$4,$2
1:	jal	$2		# can't swap with move
		
	move	$4,$2
1:	jr	$2		# can't swap with move
	
	move	$4,$2
1:	jr	$31		# can't swap with move
	
	seb	$4
	seh	$4
	zeb	$4
	zeh	$4

	save	$31,8
	save	$31,128
	save	$31,$16,16
	save	$31,$16-$17,16
	save	$31,$17,120
	save	$31,$16,136
	save	$4,$31,$16-$17,16
	save	$4-$5,$31,$16,$18,$19,$20,16
	save	$4-$6,$31,$16-$20,16
	save	$4-$7,$31,$17,$18-$30,16
	save	$4-$5,$31,$16,$18,$19,$20,16,$6-$7
	
	.p2align 4
