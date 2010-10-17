	.text
	.global l_lo
l_lo:
	l.addi	r1, r1, lo(0xdeadbeef)
	.global	l_hi
l_hi:	
	l.movhi	r1, hi(0xdeadbeef)
