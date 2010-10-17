# extw test

	extw r0,r1
	extw fp,sp

	extw r0,0
	extw r1,-1
	extw 0,r2
	extw -1,r3
	extw r4,255
	extw 255,r5
	extw r6,-256
	extw -256,r7

	extw r8,256
	extw r9,-257
	extw r11,0x42424242

	extw 255,256

	extw r0,foo

	extw.eq r10,r11
	extw.ne r12,r13
	extw.lt r14,0
	extw.gt r15,512

	extw.f r0,r1
	extw.f r2,1
	extw.f 0,r4
	extw.f r5,512
	extw.f 512,512

	extw.eq.f r0,r1
	extw.ne.f r1,0
	extw.lt.f 0,r2
	extw.le.f r0,512
	extw.n.f 512,512
