//	
// Test mutex relation handling	
//	
.text
	.explicit
start:	
	cmp.eq	p6, p0 = r29, r0
	add	r26 = r26, r29
	ld8	r29 = [r26]

	.pred.rel.mutex p1, p2
	cmp.eq p0, p1 = r1, r2;;
(p1)	mov r4 = 2
(p2)	mov r4 = 4
	rfi

	.pred.rel.mutex p1, p2
(p3)	cmp.eq p0, p1 = r1, r2;;
(p1)	mov r4 = 2
(p2)	mov r4 = 4
	rfi

	.pred.rel.mutex p1, p2
	cmp.eq p2, p3 = r1, r2;;
(p1)	mov r4 = 2
(p2)	mov r4 = 4
	rfi

	.pred.rel.mutex p1, p2
(p3)	cmp.eq p2, p3 = r1, r2;;
(p1)	mov r4 = 2
(p2)	mov r4 = 4
	rfi
