.text
	.type _start,@function
_start:

	add r101 = r102, r103
(p1)	add r104 = r105, r106
	add r107 = r108, r109, 1
(p2)	add r110 = r111, r112, 1

	adds r20 = 0, r10
(p1)	adds r21 = 1, r10
	adds r22 = -1, r10
	adds r23 = -0x2000, r10
(p2)	adds r24 = 0x1FFF, r10

	addl r30 = 0, r1
	addl r31 = 1, r1
(p1)	addl r32 = -1, r1
	addl r33 = -0x2000, r1
	addl r34 = 0x1FFF, r1
	addl r35 = -0x200000, r1
	addl r36 = 0x1FFFFF, r1

	add r11 = 0, r10
	add r12 = 0x1234, r10
	add r13 = 0x1234, r1
	add r14 = 0x12345, r1

	addp4 r20 = r3, r10
(p1)	addp4 r21 = 1, r10
	addp4 r22 = -1, r10

	sub r101 = r102, r103
(p2)	sub r110 = r111, r112, 1
	sub r120 = 0, r3
	sub r121 = 1, r3
	sub r122 = -1, r3
	sub r123 = -128, r3
	sub r124 = 127, r3

	and r8 = r9, r10
(p3)	and r11 = -128, r12

(p4)	or r8 = r9, r10
	or r11 = -128, r12

	xor r8 = r9, r10
	xor r11 = -128, r12

	andcm r8 = r9, r10
	andcm r11 = -128, r12

	shladd r8 = r30, 1, r31
	shladd r9 = r30, 2, r31
	shladd r10 = r30, 3, r31
	shladd r11 = r30, 4, r31

	shladdp4 r8 = r30, 1, r31
	shladdp4 r9 = r30, 2, r31
	shladdp4 r10 = r30, 3, r31
	shladdp4 r11 = r30, 4, r31

	padd1 r10 = r30, r31
	padd1.sss r11 = r30, r31
	padd1.uus r12 = r30, r31
	padd1.uuu r13 = r30, r31
	padd2 r14 = r30, r31
	padd2.sss r15 = r30, r31
	padd2.uus r16 = r30, r31
	padd2.uuu r17 = r30, r31
	padd4 r18 = r30, r31

	psub1 r10 = r30, r31
	psub1.sss r11 = r30, r31
	psub1.uus r12 = r30, r31
	psub1.uuu r13 = r30, r31
	psub2 r14 = r30, r31
	psub2.sss r15 = r30, r31
	psub2.uus r16 = r30, r31
	psub2.uuu r17 = r30, r31
	psub4 r18 = r30, r31

	pavg1 r10 = r30, r31
	pavg1.raz r10 = r30, r31
	pavg2 r10 = r30, r31
	pavg2.raz r10 = r30, r31

	pavgsub1 r10 = r30, r31
	pavgsub2 r10 = r30, r31

	pcmp1.eq r10 = r30, r31
	pcmp2.eq r10 = r30, r31
	pcmp4.eq r10 = r30, r31
	pcmp1.gt r10 = r30, r31
	pcmp2.gt r10 = r30, r31
	pcmp4.gt r10 = r30, r31

	pshladd2 r10 = r11, 1, r12
	pshladd2 r10 = r11, 3, r12

	pshradd2 r10 = r11, 1, r12
	pshradd2 r10 = r11, 2, r12

	cmp.eq p2, p3 = r3, r4
	cmp.eq p2, p3 = 3, r4
	cmp.ne p2, p3 = r3, r4
	cmp.ne p2, p3 = 3, r4
	cmp.lt p2, p3 = r3, r4
	cmp.lt p2, p3 = 3, r4
	cmp.le p2, p3 = r3, r4
	cmp.le p2, p3 = 3, r4
	cmp.gt p2, p3 = r3, r4
	cmp.gt p2, p3 = 3, r4
	cmp.ge p2, p3 = r3, r4
	cmp.ge p2, p3 = 3, r4
	cmp.ltu p2, p3 = r3, r4
	cmp.ltu p2, p3 = 3, r4
	cmp.leu p2, p3 = r3, r4
	cmp.leu p2, p3 = 3, r4
	cmp.gtu p2, p3 = r3, r4
	cmp.gtu p2, p3 = 3, r4
	cmp.geu p2, p3 = r3, r4
	cmp.geu p2, p3 = 3, r4

	cmp.eq.unc p2, p3 = r3, r4
	cmp.eq.unc p2, p3 = 3, r4
	cmp.ne.unc p2, p3 = r3, r4
	cmp.ne.unc p2, p3 = 3, r4
	cmp.lt.unc p2, p3 = r3, r4
	cmp.lt.unc p2, p3 = 3, r4
	cmp.le.unc p2, p3 = r3, r4
	cmp.le.unc p2, p3 = 3, r4
	cmp.gt.unc p2, p3 = r3, r4
	cmp.gt.unc p2, p3 = 3, r4
	cmp.ge.unc p2, p3 = r3, r4
	cmp.ge.unc p2, p3 = 3, r4
	cmp.ltu.unc p2, p3 = r3, r4
	cmp.ltu.unc p2, p3 = 3, r4
	cmp.leu.unc p2, p3 = r3, r4
	cmp.leu.unc p2, p3 = 3, r4
	cmp.gtu.unc p2, p3 = r3, r4
	cmp.gtu.unc p2, p3 = 3, r4
	cmp.geu.unc p2, p3 = r3, r4
	cmp.geu.unc p2, p3 = 3, r4

	cmp.eq.and p2, p3 = r3, r4
	cmp.eq.and p2, p3 = 3, r4
	cmp.eq.or p2, p3 = r3, r4
	cmp.eq.or p2, p3 = 3, r4
	cmp.eq.or.andcm p2, p3 = r3, r4
	cmp.eq.or.andcm p2, p3 = 3, r4
	cmp.eq.orcm p2, p3 = r3, r4
	cmp.eq.orcm p2, p3 = 3, r4
	cmp.eq.andcm p2, p3 = r3, r4
	cmp.eq.andcm p2, p3 = 3, r4
	cmp.eq.and.orcm p2, p3 = r3, r4
	cmp.eq.and.orcm p2, p3 = 3, r4

	cmp.ne.and p2, p3 = r3, r4
	cmp.ne.and p2, p3 = 3, r4
	cmp.ne.or p2, p3 = r3, r4
	cmp.ne.or p2, p3 = 3, r4
	cmp.ne.or.andcm p2, p3 = r3, r4
	cmp.ne.or.andcm p2, p3 = 3, r4
	cmp.ne.orcm p2, p3 = r3, r4
	cmp.ne.orcm p2, p3 = 3, r4
	cmp.ne.andcm p2, p3 = r3, r4
	cmp.ne.andcm p2, p3 = 3, r4
	cmp.ne.and.orcm p2, p3 = r3, r4
	cmp.ne.and.orcm p2, p3 = 3, r4

	cmp.eq.and p2, p3 = r0, r4
	cmp.eq.and p2, p3 = r4, r0
	cmp.eq.or p2, p3 = r0, r4
	cmp.eq.or p2, p3 = r4, r0
	cmp.eq.or.andcm p2, p3 = r0, r4
	cmp.eq.or.andcm p2, p3 = r4, r0
	cmp.eq.orcm p2, p3 = r0, r4
	cmp.eq.orcm p2, p3 = r4, r0
	cmp.eq.andcm p2, p3 = r0, r4
	cmp.eq.andcm p2, p3 = r4, r0
	cmp.eq.and.orcm p2, p3 = r0, r4
	cmp.eq.and.orcm p2, p3 = r4, r0

	cmp.ne.and p2, p3 = r0, r4
	cmp.ne.and p2, p3 = r4, r0
	cmp.ne.or p2, p3 = r0, r4
	cmp.ne.or p2, p3 = r4, r0
	cmp.ne.or.andcm p2, p3 = r0, r4
	cmp.ne.or.andcm p2, p3 = r4, r0
	cmp.ne.orcm p2, p3 = r0, r4
	cmp.ne.orcm p2, p3 = r4, r0
	cmp.ne.andcm p2, p3 = r0, r4
	cmp.ne.andcm p2, p3 = r4, r0
	cmp.ne.and.orcm p2, p3 = r0, r4
	cmp.ne.and.orcm p2, p3 = r4, r0

	cmp.lt.and p2, p3 = r0, r4
	cmp.lt.and p2, p3 = r4, r0
	cmp.lt.or p2, p3 = r0, r4
	cmp.lt.or p2, p3 = r4, r0
	cmp.lt.or.andcm p2, p3 = r0, r4
	cmp.lt.or.andcm p2, p3 = r4, r0
	cmp.lt.orcm p2, p3 = r0, r4
	cmp.lt.orcm p2, p3 = r4, r0
	cmp.lt.andcm p2, p3 = r0, r4
	cmp.lt.andcm p2, p3 = r4, r0
	cmp.lt.and.orcm p2, p3 = r0, r4
	cmp.lt.and.orcm p2, p3 = r4, r0

	cmp.le.and p2, p3 = r0, r4
	cmp.le.and p2, p3 = r4, r0
	cmp.le.or p2, p3 = r0, r4
	cmp.le.or p2, p3 = r4, r0
	cmp.le.or.andcm p2, p3 = r0, r4
	cmp.le.or.andcm p2, p3 = r4, r0
	cmp.le.orcm p2, p3 = r0, r4
	cmp.le.orcm p2, p3 = r4, r0
	cmp.le.andcm p2, p3 = r0, r4
	cmp.le.andcm p2, p3 = r4, r0
	cmp.le.and.orcm p2, p3 = r0, r4
	cmp.le.and.orcm p2, p3 = r4, r0

	cmp.gt.and p2, p3 = r0, r4
	cmp.gt.and p2, p3 = r4, r0
	cmp.gt.or p2, p3 = r0, r4
	cmp.gt.or p2, p3 = r4, r0
	cmp.gt.or.andcm p2, p3 = r0, r4
	cmp.gt.or.andcm p2, p3 = r4, r0
	cmp.gt.orcm p2, p3 = r0, r4
	cmp.gt.orcm p2, p3 = r4, r0
	cmp.gt.andcm p2, p3 = r0, r4
	cmp.gt.andcm p2, p3 = r4, r0
	cmp.gt.and.orcm p2, p3 = r0, r4
	cmp.gt.and.orcm p2, p3 = r4, r0

	cmp.ge.and p2, p3 = r0, r4
	cmp.ge.and p2, p3 = r4, r0
	cmp.ge.or p2, p3 = r0, r4
	cmp.ge.or p2, p3 = r4, r0
	cmp.ge.or.andcm p2, p3 = r0, r4
	cmp.ge.or.andcm p2, p3 = r4, r0
	cmp.ge.orcm p2, p3 = r0, r4
	cmp.ge.orcm p2, p3 = r4, r0
	cmp.ge.andcm p2, p3 = r0, r4
	cmp.ge.andcm p2, p3 = r4, r0
	cmp.ge.and.orcm p2, p3 = r0, r4
	cmp.ge.and.orcm p2, p3 = r4, r0

	cmp4.eq p2, p3 = r3, r4
	cmp4.eq p2, p3 = 3, r4
	cmp4.ne p2, p3 = r3, r4
	cmp4.ne p2, p3 = 3, r4
	cmp4.lt p2, p3 = r3, r4
	cmp4.lt p2, p3 = 3, r4
	cmp4.le p2, p3 = r3, r4
	cmp4.le p2, p3 = 3, r4
	cmp4.gt p2, p3 = r3, r4
	cmp4.gt p2, p3 = 3, r4
	cmp4.ge p2, p3 = r3, r4
	cmp4.ge p2, p3 = 3, r4
	cmp4.ltu p2, p3 = r3, r4
	cmp4.ltu p2, p3 = 3, r4
	cmp4.leu p2, p3 = r3, r4
	cmp4.leu p2, p3 = 3, r4
	cmp4.gtu p2, p3 = r3, r4
	cmp4.gtu p2, p3 = 3, r4
	cmp4.geu p2, p3 = r3, r4
	cmp4.geu p2, p3 = 3, r4

	cmp4.eq.unc p2, p3 = r3, r4
	cmp4.eq.unc p2, p3 = 3, r4
	cmp4.ne.unc p2, p3 = r3, r4
	cmp4.ne.unc p2, p3 = 3, r4
	cmp4.lt.unc p2, p3 = r3, r4
	cmp4.lt.unc p2, p3 = 3, r4
	cmp4.le.unc p2, p3 = r3, r4
	cmp4.le.unc p2, p3 = 3, r4
	cmp4.gt.unc p2, p3 = r3, r4
	cmp4.gt.unc p2, p3 = 3, r4
	cmp4.ge.unc p2, p3 = r3, r4
	cmp4.ge.unc p2, p3 = 3, r4
	cmp4.ltu.unc p2, p3 = r3, r4
	cmp4.ltu.unc p2, p3 = 3, r4
	cmp4.leu.unc p2, p3 = r3, r4
	cmp4.leu.unc p2, p3 = 3, r4
	cmp4.gtu.unc p2, p3 = r3, r4
	cmp4.gtu.unc p2, p3 = 3, r4
	cmp4.geu.unc p2, p3 = r3, r4
	cmp4.geu.unc p2, p3 = 3, r4

	cmp4.eq.and p2, p3 = r3, r4
	cmp4.eq.and p2, p3 = 3, r4
	cmp4.eq.or p2, p3 = r3, r4
	cmp4.eq.or p2, p3 = 3, r4
	cmp4.eq.or.andcm p2, p3 = r3, r4
	cmp4.eq.or.andcm p2, p3 = 3, r4
	cmp4.eq.orcm p2, p3 = r3, r4
	cmp4.eq.orcm p2, p3 = 3, r4
	cmp4.eq.andcm p2, p3 = r3, r4
	cmp4.eq.andcm p2, p3 = 3, r4
	cmp4.eq.and.orcm p2, p3 = r3, r4
	cmp4.eq.and.orcm p2, p3 = 3, r4

	cmp4.ne.and p2, p3 = r3, r4
	cmp4.ne.and p2, p3 = 3, r4
	cmp4.ne.or p2, p3 = r3, r4
	cmp4.ne.or p2, p3 = 3, r4
	cmp4.ne.or.andcm p2, p3 = r3, r4
	cmp4.ne.or.andcm p2, p3 = 3, r4
	cmp4.ne.orcm p2, p3 = r3, r4
	cmp4.ne.orcm p2, p3 = 3, r4
	cmp4.ne.andcm p2, p3 = r3, r4
	cmp4.ne.andcm p2, p3 = 3, r4
	cmp4.ne.and.orcm p2, p3 = r3, r4
	cmp4.ne.and.orcm p2, p3 = 3, r4

	cmp4.eq.and p2, p3 = r0, r4
	cmp4.eq.and p2, p3 = r4, r0
	cmp4.eq.or p2, p3 = r0, r4
	cmp4.eq.or p2, p3 = r4, r0
	cmp4.eq.or.andcm p2, p3 = r0, r4
	cmp4.eq.or.andcm p2, p3 = r4, r0
	cmp4.eq.orcm p2, p3 = r0, r4
	cmp4.eq.orcm p2, p3 = r4, r0
	cmp4.eq.andcm p2, p3 = r0, r4
	cmp4.eq.andcm p2, p3 = r4, r0
	cmp4.eq.and.orcm p2, p3 = r0, r4
	cmp4.eq.and.orcm p2, p3 = r4, r0

	cmp4.ne.and p2, p3 = r0, r4
	cmp4.ne.and p2, p3 = r4, r0
	cmp4.ne.or p2, p3 = r0, r4
	cmp4.ne.or p2, p3 = r4, r0
	cmp4.ne.or.andcm p2, p3 = r0, r4
	cmp4.ne.or.andcm p2, p3 = r4, r0
	cmp4.ne.orcm p2, p3 = r0, r4
	cmp4.ne.orcm p2, p3 = r4, r0
	cmp4.ne.andcm p2, p3 = r0, r4
	cmp4.ne.andcm p2, p3 = r4, r0
	cmp4.ne.and.orcm p2, p3 = r0, r4
	cmp4.ne.and.orcm p2, p3 = r4, r0

	cmp4.lt.and p2, p3 = r0, r4
	cmp4.lt.and p2, p3 = r4, r0
	cmp4.lt.or p2, p3 = r0, r4
	cmp4.lt.or p2, p3 = r4, r0
	cmp4.lt.or.andcm p2, p3 = r0, r4
	cmp4.lt.or.andcm p2, p3 = r4, r0
	cmp4.lt.orcm p2, p3 = r0, r4
	cmp4.lt.orcm p2, p3 = r4, r0
	cmp4.lt.andcm p2, p3 = r0, r4
	cmp4.lt.andcm p2, p3 = r4, r0
	cmp4.lt.and.orcm p2, p3 = r0, r4
	cmp4.lt.and.orcm p2, p3 = r4, r0

	cmp4.le.and p2, p3 = r0, r4
	cmp4.le.and p2, p3 = r4, r0
	cmp4.le.or p2, p3 = r0, r4
	cmp4.le.or p2, p3 = r4, r0
	cmp4.le.or.andcm p2, p3 = r0, r4
	cmp4.le.or.andcm p2, p3 = r4, r0
	cmp4.le.orcm p2, p3 = r0, r4
	cmp4.le.orcm p2, p3 = r4, r0
	cmp4.le.andcm p2, p3 = r0, r4
	cmp4.le.andcm p2, p3 = r4, r0
	cmp4.le.and.orcm p2, p3 = r0, r4
	cmp4.le.and.orcm p2, p3 = r4, r0

	cmp4.gt.and p2, p3 = r0, r4
	cmp4.gt.and p2, p3 = r4, r0
	cmp4.gt.or p2, p3 = r0, r4
	cmp4.gt.or p2, p3 = r4, r0
	cmp4.gt.or.andcm p2, p3 = r0, r4
	cmp4.gt.or.andcm p2, p3 = r4, r0
	cmp4.gt.orcm p2, p3 = r0, r4
	cmp4.gt.orcm p2, p3 = r4, r0
	cmp4.gt.andcm p2, p3 = r0, r4
	cmp4.gt.andcm p2, p3 = r4, r0
	cmp4.gt.and.orcm p2, p3 = r0, r4
	cmp4.gt.and.orcm p2, p3 = r4, r0

	cmp4.ge.and p2, p3 = r0, r4
	cmp4.ge.and p2, p3 = r4, r0
	cmp4.ge.or p2, p3 = r0, r4
	cmp4.ge.or p2, p3 = r4, r0
	cmp4.ge.or.andcm p2, p3 = r0, r4
	cmp4.ge.or.andcm p2, p3 = r4, r0
	cmp4.ge.orcm p2, p3 = r0, r4
	cmp4.ge.orcm p2, p3 = r4, r0
	cmp4.ge.andcm p2, p3 = r0, r4
	cmp4.ge.andcm p2, p3 = r4, r0
	cmp4.ge.and.orcm p2, p3 = r0, r4
	cmp4.ge.and.orcm p2, p3 = r4, r0

nop.i 0; nop.i 0
