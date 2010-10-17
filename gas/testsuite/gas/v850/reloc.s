	.text
	movea lo(foo),r0,r1
	movhi hi(foo),r1,r1
	movhi hi0(foo),r1,r1
	movea zdaoff(_foo),r0,r1
	movhi tdaoff(_foo),ep,r1
	movhi sdaoff(_foo),gp,r1
