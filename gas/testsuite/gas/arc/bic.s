# bic test

	bic r0,r1,r2
	bic r26,fp,sp
	bic ilink1,ilink2,blink
	bic r56,r59,lp_count

	bic r0,r1,0
	bic r0,0,r2
	bic 0,r1,r2
	bic r0,r1,-1
	bic r0,-1,r2
	bic -1,r1,r2
	bic r0,r1,255
	bic r0,255,r2
	bic 255,r1,r2
	bic r0,r1,-256
	bic r0,-256,r2
	bic -256,r1,r2

	bic r0,r1,256
	bic r0,-257,r2

	bic r0,255,256
	bic r0,256,255

	bic r0,r1,foo

	bic.al r0,r1,r2
	bic.ra r3,r4,r5
	bic.eq r6,r7,r8
	bic.z r9,r10,r11
	bic.ne r12,r13,r14
	bic.nz r15,r16,r17
	bic.pl r18,r19,r20
	bic.p r21,r22,r23
	bic.mi r24,r25,r26
	bic.n r27,r28,r29
	bic.cs r30,r31,r32
	bic.c r33,r34,r35
	bic.lo r36,r37,r38
	bic.cc r39,r40,r41
	bic.nc r42,r43,r44
	bic.hs r45,r46,r47
	bic.vs r48,r49,r50
	bic.v r56,r52,r53
	bic.vc r56,r55,r56
	bic.nv r56,r58,r59
	bic.gt r60,r60,r0
	bic.ge r0,r0,0
	bic.lt r1,1,r1
	bic.hi r3,3,r3
	bic.ls 4,4,r4
	bic.pnz 5,r5,5

	bic.f r0,r1,r2
	bic.f r0,r1,1
	bic.f r0,1,r2
	bic.f 0,r1,r2
	bic.f r0,r1,512
	bic.f r0,512,r2

	bic.eq.f r0,r1,r2
	bic.ne.f r0,r1,0
	bic.lt.f r0,0,r2
	bic.gt.f 0,r1,r2
	bic.le.f r0,r1,512
	bic.ge.f r0,512,r2
