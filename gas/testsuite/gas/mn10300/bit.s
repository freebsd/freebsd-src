	.text
	btst 64,d1
	btst 8192,d2
	btst 131071,d3
	btst 64,(8,a1)
	btst 64,(131071)
	bset d1,(a2)
	bset 64,(8,a1)
	bset 64,(131071)
	bclr d1,(a2)
	bclr 64,(8,a1)
	bclr 64,(131071)
