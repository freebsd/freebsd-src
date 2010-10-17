//	
// Test mutex relation handling	
//	
.text
start:	
// user annotation	
	.pred.rel.mutex p1, p2, p3
(p1)	mov r4 = 2
(p2)	mov r4 = 5
(p3)	mov r4 = 7
	rfi

// non-predicated compares generate a mutex
	cmp.eq	p1, p2 = r1, r2;;
(p1)	mov r4 = 2
(p2)	mov r4 = 4
	rfi

// unconditional compares generate a mutex
(p3)	cmp.eq.unc p1, p2 = r1, r2;;
(p1)	mov r4 = 2
(p2)	mov r4 = 4
	rfi

// non-predicated compares don't remove mutex
	cmp.eq p1, p2 = r1, r2;;
(p1)	mov r4 = 2
(p2)	mov r4 = 4
	rfi

// predicated compares don't remove mutex
(p3)	cmp.eq p1, p2 = r1, r2;;
(p1)	mov r4 = 2
(p2)	mov r4 = 4
	rfi
L:	
