//	
// Test predicate safety across calls
//	
.text
start:	
// user annotation	
	.pred.safe_across_calls p1-p4
	.pred.safe_across_calls p1,p2,p3,p4
	.pred.safe_across_calls p1-p2,p3-p4
	.pred.safe_across_calls p1-p3,p4
	cmp.eq	p1, p2 = r1, r2
	cmp.eq	p3, p4 = r3, r4 ;;
	
(p3)	br.call.sptk	b1 = L
(p1)	mov	r4 = 2
(p2)	mov	r4 = 5
(p3)	mov	r5 = r6
(p4)	mov	r5 = r7
L:	
