// A larger dual-mode test, from the programmer's reference manual.
// This uses Intel syntax, as in the manual.

// Single-precision vector sum
	fld.d	r0(r16),f20
	mov	-2,r21
	d.pfadd.ss	f0,f0,f0
	adds	-6,r17,r17
	d.pfadd.ss	f0,f0,f0
	bla	r21,r17,L1
	d.pfadd.ss	f0,f0,f0
	fld.d	8(r16)++,f22
L1:
	d.pfadd.ss	f20,f30,f30
	bla	r21,r17,L2
	d.pfadd.ss	f21,f31,f31
	fld.d	8(r16)++,f20
	d.pfadd.ss	f20,f30,f30
	br	S
	d.pfadd.ss	f21,f31,f31
	nop
L2:
	d.pfadd.ss	f22,f30,f30
	bla	r21,r17,L1
	d.pfadd.ss	f23,f31,f31
	fld.d	8(r16)++,f22
	d.pfadd.ss	f20,f30,f30
	nop
	d.pfadd.ss	f21,f31,f31
	nop
S:
	pfadd.ss	f22,f30,f30
	mov	-4,r21
	pfadd.ss	f23,f31,f31
	bte	r21,r17,DONE
	fld.l	8(r16)++,f20
	pfadd.ss	f20,f30,f30
DONE:
	pfadd.ss	f0,f0,f30
	pfadd.ss	f30,f31,f31
	pfadd.ss	f0,f0,f30
	pfadd.ss	f0,f0,f0
	pfadd.ss	f0,f0,f31
	fadd.ss		f30,f31,f16

	
