	.text
h8300_logical:
	and #16,r1l
	and r1l,r1h
	andc #16,ccr
	or #16,r0l
	or r1l,r0l
	orc #16,ccr
	xor #16,r0l
	xor r0l,r1l
	xorc #16,ccr
	neg r0l
	not r0l

