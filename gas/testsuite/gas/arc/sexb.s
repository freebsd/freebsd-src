# sexb test

	sexb r0,r1
	sexb fp,sp

	sexb r0,0
	sexb r1,-1
	sexb 0,r2
	sexb -1,r3
	sexb r4,255
	sexb 255,r5
	sexb r6,-256
	sexb -256,r7

	sexb r8,256
	sexb r9,-257
	sexb r11,0x42424242

	sexb 255,256

	sexb r0,foo

	sexb.eq r10,r11
	sexb.ne r12,r13
	sexb.lt r14,0
	sexb.gt r15,512

	sexb.f r0,r1
	sexb.f r2,1
	sexb.f 0,r4
	sexb.f r5,512
	sexb.f 512,512

	sexb.eq.f r0,r1
	sexb.ne.f r1,0
	sexb.lt.f 0,r2
	sexb.le.f r0,512
	sexb.n.f 512,512
