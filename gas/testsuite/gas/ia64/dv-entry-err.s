//
// Verify DV detection on multiple paths
//			
.text
	.explicit
// WAW on r4 is avoided on both paths
// WAW on r5 is avoided on path 0 (from top) but not path 1 (from L)
	cmp.eq	p1, p2 = r1, r2
	cmp.eq	p3, p4 = r3, r0;;
(p1)	mov	r4 = 2
L:	
(p2)	mov	r4 = 5
(p3)	mov	r5 = r7
(p4)	mov	r5 = r8

