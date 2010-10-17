	.h8300h
	.text
h8300h_cmp:
	cmp.b #0,r0l
	cmp.b r0h,r0l
	cmp.w #32,r0
	cmp.w r0,r1
	cmp.l #64,er0
	cmp.l er0,er1

