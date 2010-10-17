# mov test

	mov r0,r1
	mov fp,sp

	mov r0,0
	mov r1,-1
	mov 0,r2
	mov -1,r3
	mov r4,255
	mov 255,r5
	mov r6,-256
	mov -256,r7

	mov r8,256
	mov r9,-257
	mov r11,0x42424242

	mov 255,256

	mov r0,foo

	mov.al r0,r1
	mov.ra r3,r4
	mov.eq r6,r7
	mov.z r9,r10
	mov.ne r12,r13
	mov.nz r15,r16
	mov.pl r18,r19
	mov.p r21,r22
	mov.mi r24,r25
	mov.n r27,r28
	mov.cs r30,r31
	mov.c r33,r34
	mov.lo r36,r37
	mov.cc r39,r40
	mov.nc r42,r43
	mov.hs r45,r46
	mov.vs r48,r49
	mov.v r49,r50
	mov.vc r49,r55
	mov.nv r49,r58
	mov.gt r60,r60
	mov.ge r0,0
	mov.le 2,2
	mov.hi r3,r3
	mov.ls r4,r4
	mov.pnz r5,r5

	mov.f r0,r1
	mov.f r2,1
	mov.f 1,r3
	mov.f 0,r4
	mov.f r5,512
	mov.f 512,512

	mov.eq.f r0,r1
	mov.ne.f r1,0
