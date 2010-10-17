	.h8300h
	.text
h8300h_incdec:
	dec.b r0l
	dec.w #1,r0
	dec.w #2,r0
	dec.l #1,er0
	dec.l #2,er0
	inc.b r0l
	inc.w #1,r0
	inc.w #2,r0
	inc.l #1,er0
	inc.l #2,er0

