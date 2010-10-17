# and test

	and r0,r1,r2
	and r26,fp,sp
	and ilink1,ilink2,blink
	and r56,r59,lp_count

	and r0,r1,0
	and r0,0,r2
	and 0,r1,r2
	and r0,r1,-1
	and r0,-1,r2
	and -1,r1,r2
	and r0,r1,255
	and r0,255,r2
	and 255,r1,r2
	and r0,r1,-256
	and r0,-256,r2
	and -256,r1,r2

	and r0,r1,256
	and r0,-257,r2

	and r0,255,256
	and r0,256,255

	and r0,r1,foo

	and.al r0,r1,r2
	and.ra r3,r4,r5
	and.eq r6,r7,r8
	and.z r9,r10,r11
	and.ne r12,r13,r14
	and.nz r15,r16,r17
	and.pl r18,r19,r20
	and.p r21,r22,r23
	and.mi r24,r25,r26
	and.n r27,r28,r29
	and.cs r30,r31,r32
	and.c r33,r34,r35
	and.lo r36,r37,r38
	and.cc r39,r40,r41
	and.nc r42,r43,r44
	and.hs r45,r46,r47
	and.vs r48,r49,r50
	and.v r56,r52,r53
	and.vc r56,r55,r56
	and.nv r56,r58,r59
	and.gt r60,r60,r0
	and.ge r0,r0,0
	and.lt r1,1,r1
	and.hi r3,3,r3
	and.ls 4,4,r4
	and.pnz 5,r5,5

	and.f r0,r1,r2
	and.f r0,r1,1
	and.f r0,1,r2
	and.f 0,r1,r2
	and.f r0,r1,512
	and.f r0,512,r2

	and.eq.f r0,r1,r2
	and.ne.f r0,r1,0
	and.lt.f r0,0,r2
	and.gt.f 0,r1,r2
	and.le.f r0,r1,512
	and.ge.f r0,512,r2
