# ror test

	ror r0,r1
	ror fp,sp

	ror r0,0
	ror r1,-1
	ror 0,r2
	ror -1,r3
	ror r4,255
	ror 255,r5
	ror r6,-256
	ror -256,r7

	ror r8,256
	ror r9,-257
	ror r11,0x42424242

	ror 255,256

	ror r0,foo

	ror.eq r10,r11
	ror.ne r12,r13
	ror.lt r14,0
	ror.gt r15,512

	ror.f r0,r1
	ror.f r2,1
	ror.f 0,r4
	ror.f r5,512
	ror.f 512,512

	ror.eq.f r0,r1
	ror.ne.f r1,0
	ror.lt.f 0,r2
	ror.le.f r0,512
	ror.n.f 512,512
