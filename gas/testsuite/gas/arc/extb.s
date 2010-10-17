# extb test

	extb r0,r1
	extb fp,sp

	extb r0,0
	extb r1,-1
	extb 0,r2
	extb -1,r3
	extb r4,255
	extb 255,r5
	extb r6,-256
	extb -256,r7

	extb r8,256
	extb r9,-257
	extb r11,0x42424242

	extb 255,256

	extb r0,foo

	extb.eq r10,r11
	extb.ne r12,r13
	extb.lt r14,0
	extb.gt r15,512

	extb.f r0,r1
	extb.f r2,1
	extb.f 0,r4
	extb.f r5,512
	extb.f 512,512

	extb.eq.f r0,r1
	extb.ne.f r1,0
	extb.lt.f 0,r2
	extb.le.f r0,512
	extb.n.f 512,512
