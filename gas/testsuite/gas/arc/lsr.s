# lsr test

	lsr r0,r1
	lsr fp,sp

	lsr r0,0
	lsr r1,-1
	lsr 0,r2
	lsr -1,r3
	lsr r4,255
	lsr 255,r5
	lsr r6,-256
	lsr -256,r7

	lsr r8,256
	lsr r9,-257
	lsr r11,0x42424242

	lsr 255,256

	lsr r0,foo

	lsr.eq r10,r11
	lsr.ne r12,r13
	lsr.lt r14,0
	lsr.gt r15,512

	lsr.f r0,r1
	lsr.f r2,1
	lsr.f 0,r4
	lsr.f r5,512
	lsr.f 512,512

	lsr.eq.f r0,r1
	lsr.ne.f r1,0
	lsr.lt.f 0,r2
	lsr.le.f r0,512
	lsr.n.f 512,512
