# Test the generation of the mips16e save instruction

	.set	mips16
	.text
func:
# Un-extended version
	save	8
	save    $31,16
	save	$16,24
	save	$17,32
	save	$16-$17,40
	save    $31,$16,48
	save    $31,$17,56
	save    $31,$16,$17,64
	save    $31,$16-$17,72
	save    80,$31,$16-$17
	save    $31,88,$16,$17
	save    $31,$17,128,$16

# Extended version
	save	136
	save    $31,144
	save	$16-$17,152

	# sreg
	save	$18,64
	save	$18-$23,72
	save	$18-$23,$30,80
	save	$16-$23,$30,88
	
	# static areg
	save    64,$7
	save    128,$7,$6
	save    256,$7,$6,$5,$4

	# areg
	save    $4,256
	save    $4,$5,128
	save    $4,$5,$6,$7,64

	# mix areg and static areg
	save    $4,128,$7
	save    $4,128,$7,$6,$5
	save    $4,$5,128,$7,$6
	save    $4,$5,$6,128,$7

	save	$4-$5,$16-$23,$30-$31,128,$6-$7

	restore	$16,$17,$31,128
	restore	$31,136
	restore	$18,64
	restore	$4-$5,$16-$23,$30-$31,128,$6-$7
	
        .p2align 4

