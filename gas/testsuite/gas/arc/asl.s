# asl test

	asl r0,r1
	asl fp,sp

	asl r0,0
	asl r1,-1
	asl 0,r2
	asl -1,r3
	asl r4,255
	asl 255,r5
	asl r6,-256
	asl -256,r7

	asl r8,256
	asl r9,-257
	asl r11,0x42424242

	asl 255,256

	asl r0,foo

	asl.al r0,r1
	asl.ra r3,r4
	asl.eq r6,r7
	asl.z r9,r10
	asl.ne r12,r13
	asl.nz r15,r16
	asl.pl r18,r19
	asl.p r21,r22
	asl.mi r24,r25
	asl.n r27,r28
	asl.cs r30,r31
	asl.c r33,r34
	asl.lo r36,r37
	asl.cc r39,r40
	asl.nc r42,r43
	asl.hs r45,r46
	asl.vs r48,r49
	asl.v r49,r50
	asl.vc r49,r55
	asl.nv r49,r58
	asl.gt r60,r60
	asl.ge r0,0
	asl.le 2,2
	asl.hi r3,r3
	asl.ls r4,r4
	asl.pnz r5,r5

	asl.f r0,r1
	asl.f r2,1
	asl.f 1,r3
	asl.f 0,r4
	asl.f r5,512
	asl.f 512,512

	asl.eq.f r0,r1
	asl.ne.f r1,0
