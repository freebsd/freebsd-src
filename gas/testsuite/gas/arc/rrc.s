# rrc test

	rrc r0,r1
	rrc fp,sp

	rrc r0,0
	rrc r1,-1
	rrc 0,r2
	rrc -1,r3
	rrc r4,255
	rrc 255,r5
	rrc r6,-256
	rrc -256,r7

	rrc r8,256
	rrc r9,-257
	rrc r11,0x42424242

	rrc 255,256

	rrc r0,foo

	rrc.eq r10,r11
	rrc.ne r12,r13
	rrc.lt r14,0
	rrc.gt r15,512

	rrc.f r0,r1
	rrc.f r2,1
	rrc.f 0,r4
	rrc.f r5,512
	rrc.f 512,512

	rrc.eq.f r0,r1
	rrc.ne.f r1,0
	rrc.lt.f 0,r2
	rrc.le.f r0,512
	rrc.n.f 512,512
