//	
// Test various implies relations	
//	
.text
// User-supplied hint	
	.pred.rel.imply p1, p2
(p1)	mov		r4 = 2
(p2)	br.cond.sptk		L
	mov		r4 = 7
	rfi	
	
// Symmetric to previous example
	.pred.rel.imply p1, p2
	mov		r4 = 2
(p2)	br.cond.sptk	L
(p1)	mov		r4 = 7	
	rfi

// Verify that the implies relationship caused by the unconditional compare 
// prevents RAW on r4.  
(p3)	cmp.eq.unc	p1, p2 = r1, r2;;	// p1,p2 imply p3
(p1)	mov		r4 = 2
(p3)	br.cond.sptk	L	
	mov		r4 = 7
	rfi
	
// An instance of cmp.rel.or should not affect an implies relation.
(p3)	cmp.eq.unc	p1, p2 = r1, r2		// p1,p2 imply p3
	cmp.eq.or	p3, p4 = r5, r6;;	// doesn't affect implies rel
(p1)	mov		r4 = 2
(p3)	br.cond.sptk	L	
	mov		r4 = 7
	rfi
	
// An instance of cmp.rel.and only affects imply targets
	.pred.rel.imply p1,p3
	cmp.ne.and	p1, p2 = r5, r6		// doesn't affect imply source
(p1)	mov		r4 = 2
(p3)	br.cond.sptk	L	
	mov		r4 = 7
	rfi
	
// FIXME -- add tests for and.orcm and or.andcm	
L:	
