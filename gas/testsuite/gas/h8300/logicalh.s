	.h8300h
	.text
h8300h_logical:
	and.b #16,r1l
	and.b r1l,r1h
	and.w #32,r1
	and.w r1,r1
	and.l #64,er1
	and.l er1,er1
	andc #16,ccr
	or.b #16,r0l
	or.b r1l,r0l
	or.w #32,r1
	or.w r1,r1
	or.l #64,er1
	or.l er1,er1
	orc #16,ccr
	xor.b #16,r0l
	xor.b r0l,r1l
	xor.w #32,r1
	xor.w r1,r1
	xor.l #64,er1
	xor.l er1,er1
	xorc #16,ccr
	neg.b r0l
	neg.w r0
	neg.l er0
	not.b r0l
	not.w r0
	not.l er0

