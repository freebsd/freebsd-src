# sbc test

	sbc r0,r1,r2
	sbc r26,fp,sp
	sbc ilink1,ilink2,blink
	sbc r56,r59,lp_count

	sbc r0,r1,0
	sbc r0,0,r2
	sbc 0,r1,r2
	sbc r0,r1,-1
	sbc r0,-1,r2
	sbc -1,r1,r2
	sbc r0,r1,255
	sbc r0,255,r2
	sbc 255,r1,r2
	sbc r0,r1,-256
	sbc r0,-256,r2
	sbc -256,r1,r2

	sbc r0,r1,256
	sbc r0,-257,r2

	sbc r0,255,256
	sbc r0,256,255

	sbc r0,r1,foo

	sbc.al r0,r1,r2
	sbc.ra r3,r4,r5
	sbc.eq r6,r7,r8
	sbc.z r9,r10,r11
	sbc.ne r12,r13,r14
	sbc.nz r15,r16,r17
	sbc.pl r18,r19,r20
	sbc.p r21,r22,r23
	sbc.mi r24,r25,r26
	sbc.n r27,r28,r29
	sbc.cs r30,r31,r32
	sbc.c r33,r34,r35
	sbc.lo r36,r37,r38
	sbc.cc r39,r40,r41
	sbc.nc r42,r43,r44
	sbc.hs r45,r46,r47
	sbc.vs r48,r49,r50
	sbc.v r56,r52,r53
	sbc.vc r56,r55,r56
	sbc.nv r56,r58,r59
	sbc.gt r60,r60,r0
	sbc.ge r0,r0,0
	sbc.lt r1,1,r1
	sbc.hi r3,3,r3
	sbc.ls 4,4,r4
	sbc.pnz 5,r5,5

	sbc.f r0,r1,r2
	sbc.f r0,r1,1
	sbc.f r0,1,r2
	sbc.f 0,r1,r2
	sbc.f r0,r1,512
	sbc.f r0,512,r2

	sbc.eq.f r0,r1,r2
	sbc.ne.f r0,r1,0
	sbc.lt.f r0,0,r2
	sbc.gt.f 0,r1,r2
	sbc.le.f r0,r1,512
	sbc.ge.f r0,512,r2
