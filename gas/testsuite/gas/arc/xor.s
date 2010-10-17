# xor test

	xor r0,r1,r2
	xor r26,fp,sp
	xor ilink1,ilink2,blink
	xor r56,r59,lp_count

	xor r0,r1,0
	xor r0,0,r2
	xor 0,r1,r2
	xor r0,r1,-1
	xor r0,-1,r2
	xor -1,r1,r2
	xor r0,r1,255
	xor r0,255,r2
	xor 255,r1,r2
	xor r0,r1,-256
	xor r0,-256,r2
	xor -256,r1,r2

	xor r0,r1,256
	xor r0,-257,r2

	xor r0,255,256
	xor r0,256,255

	xor r0,r1,foo

	xor.al r0,r1,r2
	xor.ra r3,r4,r5
	xor.eq r6,r7,r8
	xor.z r9,r10,r11
	xor.ne r12,r13,r14
	xor.nz r15,r16,r17
	xor.pl r18,r19,r20
	xor.p r21,r22,r23
	xor.mi r24,r25,r26
	xor.n r27,r28,r29
	xor.cs r30,r31,r32
	xor.c r33,r34,r35
	xor.lo r36,r37,r38
	xor.cc r39,r40,r41
	xor.nc r42,r43,r44
	xor.hs r45,r46,r47
	xor.vs r48,r49,r50
	xor.v r56,r52,r53
	xor.vc r56,r55,r56
	xor.nv r56,r58,r59
	xor.gt r60,r60,r0
	xor.ge r0,r0,0
	xor.lt r1,1,r1
	xor.hi r3,3,r3
	xor.ls 4,4,r4
	xor.pnz 5,r5,5

	xor.f r0,r1,r2
	xor.f r0,r1,1
	xor.f r0,1,r2
	xor.f 0,r1,r2
	xor.f r0,r1,512
	xor.f r0,512,r2

	xor.eq.f r0,r1,r2
	xor.ne.f r0,r1,0
	xor.lt.f r0,0,r2
	xor.gt.f 0,r1,r2
	xor.le.f r0,r1,512
	xor.ge.f r0,512,r2
