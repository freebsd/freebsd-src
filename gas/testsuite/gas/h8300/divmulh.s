	.h8300h
	.text
h8300h_div_mul:
	divxu.b r0l,r1
	divxu.w r0,er1
	divxs.b r0l,r1
	divxs.w r0,er1
	mulxu.b r0l,r1
	mulxu.w r0,er1
	mulxs.b r0l,r1
	mulxs.w r0,er1

