	.text
h8300_bit_ops_3:
	bnot #0,r0l
	bnot #0,@r0
	bnot #0,@64:8
	bnot r1l,r0l
	bnot r1l,@r0
	bnot r1l,@64:8
	bset #0,r0l
	bset #0,@r0
	bset #0,@64:8
	bset r1l,r0l
	bset r1l,@r0
	bset r1l,@64:8

