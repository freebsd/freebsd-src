# adc test

	adc r0,r1,r2
	adc r26,fp,sp
	adc ilink1,ilink2,blink
	adc r56,r59,lp_count

	adc r0,r1,0
	adc r0,0,r2
	adc 0,r1,r2
	adc r0,r1,-1
	adc r0,-1,r2
	adc -1,r1,r2
	adc r0,r1,255
	adc r0,255,r2
	adc 255,r1,r2
	adc r0,r1,-256
	adc r0,-256,r2
	adc -256,r1,r2

	adc r0,r1,256
	adc r0,-257,r2

	adc r0,255,256
	adc r0,256,255

	adc r0,r1,foo

	adc.al r0,r1,r2
	adc.ra r3,r4,r5
	adc.eq r6,r7,r8
	adc.z r9,r10,r11
	adc.ne r12,r13,r14
	adc.nz r15,r16,r17
	adc.pl r18,r19,r20
	adc.p r21,r22,r23
	adc.mi r24,r25,r26
	adc.n r27,r28,r29
	adc.cs r30,r31,r32
	adc.c r33,r34,r35
	adc.lo r36,r37,r38
	adc.cc r39,r40,r41
	adc.nc r42,r43,r44
	adc.hs r45,r46,r47
	adc.vs r48,r49,r50
	adc.v r56,r52,r53
	adc.vc r56,r55,r56
	adc.nv r56,r58,r59
	adc.gt r60,r60,r0
	adc.ge r0,r0,0
	adc.lt r1,1,r1
	adc.hi r3,3,r3
	adc.ls 4,4,r4
	adc.pnz 5,r5,5

	adc.f r0,r1,r2
	adc.f r0,r1,1
	adc.f r0,1,r2
	adc.f 0,r1,r2
	adc.f r0,r1,512
	adc.f r0,512,r2

	adc.eq.f r0,r1,r2
	adc.ne.f r0,r1,0
	adc.lt.f r0,0,r2
	adc.gt.f 0,r1,r2
	adc.le.f r0,r1,512
	adc.ge.f r0,512,r2
