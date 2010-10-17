# rlc test

	rlc r0,r1
	rlc fp,sp

	rlc r0,0
	rlc r1,-1
	rlc 0,r2
	rlc -1,r3
	rlc r4,255
	rlc 255,r5
	rlc r6,-256
	rlc -256,r7

	rlc r8,256
	rlc r9,-257
	rlc r11,0x42424242

	rlc 255,256

	rlc r0,foo

	rlc.al r0,r1
	rlc.ra r3,r4
	rlc.eq r6,r7
	rlc.z r9,r10
	rlc.ne r12,r13
	rlc.nz r15,r16
	rlc.pl r18,r19
	rlc.p r21,r22
	rlc.mi r24,r25
	rlc.n r27,r28
	rlc.cs r30,r31
	rlc.c r33,r34
	rlc.lo r36,r37
	rlc.cc r39,r40
	rlc.nc r42,r43
	rlc.hs r45,r46
	rlc.vs r48,r49
	rlc.v r49,r50
	rlc.vc r49,r55
	rlc.nv r49,r58
	rlc.gt r60,r60
	rlc.ge r0,0
	rlc.le 2,2
	rlc.hi r3,r3
	rlc.ls r4,r4
	rlc.pnz r5,r5

	rlc.f r0,r1
	rlc.f r2,1
	rlc.f 1,r3
	rlc.f 0,r4
	rlc.f r5,512
	rlc.f 512,512

	rlc.eq.f r0,r1
	rlc.ne.f r1,0
