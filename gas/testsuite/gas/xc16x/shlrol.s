       .section .text
       .global _fun
xc16x_shlrol:	

	shl   r0,r1
	shl   r0,#a
	shr   r0,r1
	shr   r0,#a
	rol    r0,r1
	rol    r0,#a
	ror    r0,r1
	ror    r0,#a
	ashr r0,r1
	ashr r0,#a
