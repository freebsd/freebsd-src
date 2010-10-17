# asr test

	asr r0,r1
	asr fp,sp

	asr r0,0
	asr r1,-1
	asr 0,r2
	asr -1,r3
	asr r4,255
	asr 255,r5
	asr r6,-256
	asr -256,r7

	asr r8,256
	asr r9,-257
	asr r11,0x42424242

	asr 255,256

	asr r0,foo

	asr.eq r10,r11
	asr.ne r12,r13
	asr.lt r14,0
	asr.gt r15,512

	asr.f r0,r1
	asr.f r2,1
	asr.f 0,r4
	asr.f r5,512
	asr.f 512,512

	asr.eq.f r0,r1
	asr.ne.f r1,0
	asr.lt.f 0,r2
	asr.le.f r0,512
	asr.n.f 512,512
