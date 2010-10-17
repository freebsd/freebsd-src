	.level 1.1
	.code
	.align 4
; More branching instructions than you ever knew what to do with.
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
branch_tests: 
	bl branch_tests,%r2
	bl,n branch_tests,%r2
	b branch_tests
	b,n branch_tests
	gate branch_tests,%r2
	gate,n branch_tests,%r2
	blr %r4,%r2
	blr,n %r4,%r2
	blr %r4,%r0
	blr,n %r4,%r0
	bv %r0(%r2)
	bv,n %r0(%r2)
	be 0x1234(%sr1,%r2)
	be,n 0x1234(%sr1,%r2)
	ble 0x1234(%sr1,%r2)
	ble,n 0x1234(%sr1,%r2)

movb_tests: 
	movb %r4,%r26,movb_tests
	movb,= %r4,%r26,movb_tests
	movb,< %r4,%r26,movb_tests
	movb,od %r4,%r26,movb_tests
	movb,tr %r4,%r26,movb_tests
	movb,<> %r4,%r26,movb_tests
	movb,>= %r4,%r26,movb_tests
	movb,ev %r4,%r26,movb_tests
movb_nullified_tests: 
	movb,n %r4,%r26,movb_tests
	movb,=,n %r4,%r26,movb_tests
	movb,<,n %r4,%r26,movb_tests
	movb,od,n %r4,%r26,movb_tests
	movb,tr,n %r4,%r26,movb_tests
	movb,<>,n %r4,%r26,movb_tests
	movb,>=,n %r4,%r26,movb_tests
	movb,ev,n %r4,%r26,movb_tests

movib_tests: 
	movib 5,%r26,movib_tests
	movib,= 5,%r26,movib_tests
	movib,< 5,%r26,movib_tests
	movib,od 5,%r26,movib_tests
	movib,tr 5,%r26,movib_tests
	movib,<> 5,%r26,movib_tests
	movib,>= 5,%r26,movib_tests
	movib,ev 5,%r26,movib_tests
movib_nullified_tests: 
	movib,n 5,%r26,movib_tests
	movib,=,n 5,%r26,movib_tests
	movib,<,n 5,%r26,movib_tests
	movib,od,n 5,%r26,movib_tests
	movib,tr,n 5,%r26,movib_tests
	movib,<>,n 5,%r26,movib_tests
	movib,>=,n 5,%r26,movib_tests
	movib,ev,n 5,%r26,movib_tests

comb_tests: 
	comb %r0,%r4,comb_tests
	comb,= %r0,%r4,comb_tests
	comb,< %r0,%r4,comb_tests
	comb,<= %r0,%r4,comb_tests
	comb,<< %r0,%r4,comb_tests
	comb,<<= %r0,%r4,comb_tests
	comb,sv %r0,%r4,comb_tests
	comb,od %r0,%r4,comb_tests
	comb,tr %r0,%r4,comb_tests
	comb,<> %r0,%r4,comb_tests
	comb,>= %r0,%r4,comb_tests
	comb,> %r0,%r4,comb_tests
	comb,>>= %r0,%r4,comb_tests
	comb,>> %r0,%r4,comb_tests
	comb,nsv %r0,%r4,comb_tests
	comb,ev %r0,%r4,comb_tests
comb_nullified_tests: 
	comb,n %r0,%r4,comb_tests
	comb,=,n %r0,%r4,comb_tests
	comb,<,n %r0,%r4,comb_tests
	comb,<=,n %r0,%r4,comb_tests
	comb,<<,n %r0,%r4,comb_tests
	comb,<<=,n %r0,%r4,comb_tests
	comb,sv,n %r0,%r4,comb_tests
	comb,od,n %r0,%r4,comb_tests
	comb,tr,n %r0,%r4,comb_tests
	comb,<>,n %r0,%r4,comb_tests
	comb,>=,n %r0,%r4,comb_tests
	comb,>,n %r0,%r4,comb_tests
	comb,>>=,n %r0,%r4,comb_tests
	comb,>>,n %r0,%r4,comb_tests
	comb,nsv,n %r0,%r4,comb_tests
	comb,ev,n %r0,%r4,comb_tests

comib_tests: 
	comib 0,%r4,comib_tests
	comib,< 0,%r4,comib_tests
	comib,<= 0,%r4,comib_tests
	comib,<< 0,%r4,comib_tests
	comib,<<= 0,%r4,comib_tests
	comib,sv 0,%r4,comib_tests
	comib,od 0,%r4,comib_tests
	comib,tr 0,%r4,comib_tests
	comib,<> 0,%r4,comib_tests
	comib,>= 0,%r4,comib_tests
	comib,> 0,%r4,comib_tests
	comib,>>= 0,%r4,comib_tests
	comib,>> 0,%r4,comib_tests
	comib,nsv 0,%r4,comib_tests
	comib,ev 0,%r4,comb_tests

comib_nullified_tests: 
	comib,n 0,%r4,comib_tests
	comib,=,n 0,%r4,comib_tests
	comib,<,n 0,%r4,comib_tests
	comib,<=,n 0,%r4,comib_tests
	comib,<<,n 0,%r4,comib_tests
	comib,<<=,n 0,%r4,comib_tests
	comib,sv,n 0,%r4,comib_tests
	comib,od,n 0,%r4,comib_tests
	comib,tr,n 0,%r4,comib_tests
	comib,<>,n 0,%r4,comib_tests
	comib,>=,n 0,%r4,comib_tests
	comib,>,n 0,%r4,comib_tests
	comib,>>=,n 0,%r4,comib_tests
	comib,>>,n 0,%r4,comib_tests
	comib,nsv,n 0,%r4,comib_tests
	comib,ev,n 0,%r4,comib_tests



addb_tests: 
	addb %r1,%r4,addb_tests
	addb,= %r1,%r4,addb_tests
	addb,< %r1,%r4,addb_tests
	addb,<= %r1,%r4,addb_tests
	addb,nuv %r1,%r4,addb_tests
	addb,znv %r1,%r4,addb_tests
	addb,sv %r1,%r4,addb_tests
	addb,od %r1,%r4,addb_tests
	addb,tr %r1,%r4,addb_tests
	addb,<> %r1,%r4,addb_tests
	addb,>= %r1,%r4,addb_tests
	addb,> %r1,%r4,addb_tests
	addb,uv %r1,%r4,addb_tests
	addb,vnz %r1,%r4,addb_tests
	addb,nsv %r1,%r4,addb_tests
	addb,ev %r1,%r4,addb_tests
addb_nullified_tests: 
	addb,n %r1,%r4,addb_tests
	addb,=,n %r1,%r4,addb_tests
	addb,<,n %r1,%r4,addb_tests
	addb,<=,n %r1,%r4,addb_tests
	addb,nuv,n %r1,%r4,addb_tests
	addb,znv,n %r1,%r4,addb_tests
	addb,sv,n %r1,%r4,addb_tests
	addb,od,n %r1,%r4,addb_tests
	addb,tr,n %r1,%r4,addb_tests
	addb,<>,n %r1,%r4,addb_tests
	addb,>=,n %r1,%r4,addb_tests
	addb,>,n %r1,%r4,addb_tests
	addb,uv,n %r1,%r4,addb_tests
	addb,vnz,n %r1,%r4,addb_tests
	addb,nsv,n %r1,%r4,addb_tests
	addb,ev,n %r1,%r4,addb_tests

addib_tests: 
	addib -1,%r4,addib_tests
	addib,= -1,%r4,addib_tests
	addib,< -1,%r4,addib_tests
	addib,<= -1,%r4,addib_tests
	addib,nuv -1,%r4,addib_tests
	addib,znv -1,%r4,addib_tests
	addib,sv -1,%r4,addib_tests
	addib,od -1,%r4,addib_tests
	addib,tr -1,%r4,addib_tests
	addib,<> -1,%r4,addib_tests
	addib,>= -1,%r4,addib_tests
	addib,> -1,%r4,addib_tests
	addib,uv -1,%r4,addib_tests
	addib,vnz -1,%r4,addib_tests
	addib,nsv -1,%r4,addib_tests
	addib,ev -1,%r4,comb_tests

addib_nullified_tests: 
	addib,n -1,%r4,addib_tests
	addib,=,n -1,%r4,addib_tests
	addib,<,n -1,%r4,addib_tests
	addib,<=,n -1,%r4,addib_tests
	addib,nuv,n -1,%r4,addib_tests
	addib,znv,n -1,%r4,addib_tests
	addib,sv,n -1,%r4,addib_tests
	addib,od,n -1,%r4,addib_tests
	addib,tr,n -1,%r4,addib_tests
	addib,<>,n -1,%r4,addib_tests
	addib,>=,n -1,%r4,addib_tests
	addib,>,n -1,%r4,addib_tests
	addib,uv,n -1,%r4,addib_tests
	addib,vnz,n -1,%r4,addib_tests
	addib,nsv,n -1,%r4,addib_tests
	addib,ev,n -1,%r4,addib_tests


; Needs to check lots of stuff (like corner bit cases)
bb_tests: 
	bvb,< %r4,bb_tests
	bvb,>= %r4,bb_tests
	bvb,<,n %r4,bb_tests
	bvb,>=,n %r4,bb_tests
	bb,< %r4,5,bb_tests
	bb,>= %r4,5,bb_tests
	bb,<,n %r4,5,bb_tests
	bb,>=,n %r4,5,bb_tests
	
