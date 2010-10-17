# sub test

	sub r0,r1,r2
	sub r26,fp,sp
	sub ilink1,ilink2,blink
	sub r56,r59,lp_count

	sub r0,r1,0
	sub r0,0,r2
	sub 0,r1,r2
	sub r0,r1,-1
	sub r0,-1,r2
	sub -1,r1,r2
	sub r0,r1,255
	sub r0,255,r2
	sub 255,r1,r2
	sub r0,r1,-256
	sub r0,-256,r2
	sub -256,r1,r2

	sub r0,r1,256
	sub r0,-257,r2

	sub r0,255,256
	sub r0,256,255

	sub r0,r1,foo

	sub.al r0,r1,r2
	sub.ra r3,r4,r5
	sub.eq r6,r7,r8
	sub.z r9,r10,r11
	sub.ne r12,r13,r14
	sub.nz r15,r16,r17
	sub.pl r18,r19,r20
	sub.p r21,r22,r23
	sub.mi r24,r25,r26
	sub.n r27,r28,r29
	sub.cs r30,r31,r32
	sub.c r33,r34,r35
	sub.lo r36,r37,r38
	sub.cc r39,r40,r41
	sub.nc r42,r43,r44
	sub.hs r45,r46,r47
	sub.vs r48,r49,r50
	sub.v r56,r52,r53
	sub.vc r56,r55,r56
	sub.nv r56,r58,r59
	sub.gt r60,r60,r0
	sub.ge r0,r0,0
	sub.lt r1,1,r1
	sub.hi r3,3,r3
	sub.ls 4,4,r4
	sub.pnz 5,r5,5

	sub.f r0,r1,r2
	sub.f r0,r1,1
	sub.f r0,1,r2
	sub.f 0,r1,r2
	sub.f r0,r1,512
	sub.f r0,512,r2

	sub.eq.f r0,r1,r2
	sub.ne.f r0,r1,0
	sub.lt.f r0,0,r2
	sub.gt.f 0,r1,r2
	sub.le.f r0,r1,512
	sub.ge.f r0,512,r2
