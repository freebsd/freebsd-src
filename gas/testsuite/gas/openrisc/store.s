	.text
	.global l_sw
l_sw:
	l.sw	-4(r1), r1
	.global	l_lw
l_lw:	
	l.lw	r1, -100(r1)
