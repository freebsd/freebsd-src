# add test

	add r0,r1,r2
	add r26,fp,sp
	add ilink1,ilink2,blink
	add r56,r59,lp_count

	add r0,r1,0
	add r0,0,r2
	add 0,r1,r2
	add r0,r1,-1
	add r0,-1,r2
	add -1,r1,r2
	add r0,r1,255
	add r0,255,r2
	add 255,r1,r2
	add r0,r1,-256
	add r0,-256,r2
	add -256,r1,r2

	add r0,r1,256
	add r0,-257,r2

	add r0,255,256
	add r0,256,255

	add r0,r1,foo

	add.al r0,r1,r2
	add.ra r3,r4,r5
	add.eq r6,r7,r8
	add.z r9,r10,r11
	add.ne r12,r13,r14
	add.nz r15,r16,r17
	add.pl r18,r19,r20
	add.p r21,r22,r23
	add.mi r24,r25,r26
	add.n r27,r28,r29
	add.cs r30,r31,r32
	add.c r33,r34,r35
	add.lo r36,r37,r38
	add.cc r39,r40,r41
	add.nc r42,r43,r44
	add.hs r45,r46,r47
	add.vs r48,r49,r50
	add.v r56,r52,r53
	add.vc r56,r55,r56
	add.nv r56,r58,r59
	add.gt r60,r60,r0
	add.ge r0,r0,0
	add.lt r1,1,r1
	add.hi r3,3,r3
	add.ls 4,4,r4
	add.pnz 5,r5,5

	add.f r0,r1,r2
	add.f r0,r1,1
	add.f r0,1,r2
	add.f 0,r1,r2
	add.f r0,r1,512
	add.f r0,512,r2

	add.eq.f r0,r1,r2
	add.ne.f r0,r1,0
	add.lt.f r0,0,r2
	add.gt.f 0,r1,r2
	add.le.f r0,r1,512
	add.ge.f r0,512,r2
