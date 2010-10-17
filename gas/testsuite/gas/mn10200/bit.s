	.text
	btst 64,d1
	btst 8192,d2
	bset d1,(a2)
	bclr d1,(a2)
