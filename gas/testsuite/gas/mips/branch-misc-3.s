	# ctc1s and compares shouldn't appear in a branch delay slot.
	ctc1	$4,$31
	b	1f
1:
	ctc1	$4,$31
	bc1t	1f
1:
	c.eq.s	$f0,$f2
	b	1f
1:
	c.eq.s	$f0,$f2
	bc1t	1f
1:

	# The next three branches should have nop-filled slots.
	ctc1	$4,$31
	addiu	$5,$5,1
	bc1t	1f
1:
	ctc1	$4,$31
	addiu	$5,$5,1
	addiu	$6,$6,1
	bc1t	1f
1:
	c.eq.s	$f0,$f2
	addiu	$5,$5,1
	bc1t	1f
1:

	# ...but a swap is possible in these three.
	ctc1	$4,$31
	addiu	$5,$5,1
	addiu	$6,$6,1
	addiu	$7,$7,1
	bc1t	1f
1:
	c.eq.s	$f0,$f2
	addiu	$5,$5,1
	addiu	$6,$6,1
	bc1t	1f
1:
	addiu	$7,$7,1
	bc1t	1f
1:
