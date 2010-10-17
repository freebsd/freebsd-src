# sexw test

	sexw r0,r1
	sexw fp,sp

	sexw r0,0
	sexw r1,-1
	sexw 0,r2
	sexw -1,r3
	sexw r4,255
	sexw 255,r5
	sexw r6,-256
	sexw -256,r7

	sexw r8,256
	sexw r9,-257
	sexw r11,0x42424242

	sexw 255,256

	sexw r0,foo

	sexw.eq r10,r11
	sexw.ne r12,r13
	sexw.lt r14,0
	sexw.gt r15,512

	sexw.f r0,r1
	sexw.f r2,1
	sexw.f 0,r4
	sexw.f r5,512
	sexw.f 512,512

	sexw.eq.f r0,r1
	sexw.ne.f r1,0
	sexw.lt.f 0,r2
	sexw.le.f r0,512
	sexw.n.f 512,512
