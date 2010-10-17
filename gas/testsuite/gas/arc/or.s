# or test

	or r0,r1,r2
	or r26,fp,sp
	or ilink1,ilink2,blink
	or r56,r59,lp_count

	or r0,r1,0
	or r0,0,r2
	or 0,r1,r2
	or r0,r1,-1
	or r0,-1,r2
	or -1,r1,r2
	or r0,r1,255
	or r0,255,r2
	or 255,r1,r2
	or r0,r1,-256
	or r0,-256,r2
	or -256,r1,r2

	or r0,r1,256
	or r0,-257,r2

	or r0,255,256
	or r0,256,255

	or r0,r1,foo

	or.al r0,r1,r2
	or.ra r3,r4,r5
	or.eq r6,r7,r8
	or.z r9,r10,r11
	or.ne r12,r13,r14
	or.nz r15,r16,r17
	or.pl r18,r19,r20
	or.p r21,r22,r23
	or.mi r24,r25,r26
	or.n r27,r28,r29
	or.cs r30,r31,r32
	or.c r33,r34,r35
	or.lo r36,r37,r38
	or.cc r39,r40,r41
	or.nc r42,r43,r44
	or.hs r45,r46,r47
	or.vs r48,r49,r50
	or.v r56,r52,r53
	or.vc r56,r55,r56
	or.nv r56,r58,r59
	or.gt r60,r60,r0
	or.ge r0,r0,0
	or.lt r1,1,r1
	or.hi r3,3,r3
	or.ls 4,4,r4
	or.pnz 5,r5,5

	or.f r0,r1,r2
	or.f r0,r1,1
	or.f r0,1,r2
	or.f 0,r1,r2
	or.f r0,r1,512
	or.f r0,512,r2

	or.eq.f r0,r1,r2
	or.ne.f r0,r1,0
	or.lt.f r0,0,r2
	or.gt.f 0,r1,r2
	or.le.f r0,r1,512
	or.ge.f r0,512,r2
