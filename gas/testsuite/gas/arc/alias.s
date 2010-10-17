# @OC@ test

# reg,reg
	@OC@ r0,r1
	@OC@ fp,sp

# shimm values
	@OC@ r0,0
	@OC@ r1,-1
	@OC@ 0,r2
	@OC@ -1,r3
	@OC@ r4,255
	@OC@ 255,r5
	@OC@ r6,-256
	@OC@ -256,r7

# limm values
	@OC@ r8,256
	@OC@ r9,-257
	@OC@ 511,r10
	@OC@ r11,0x42424242
	@OC@ 0x12345678,r12

# shimm and limm
	@OC@ 255,256
	@OC@ 256,255

# symbols
	@OC@ r0,foo

# conditional execution
	@OC@.al r0,r1
	@OC@.ra r3,r4
	@OC@.eq r6,r7
	@OC@.z r9,r10
	@OC@.ne r12,r13
	@OC@.nz r15,r16
	@OC@.pl r18,r19
	@OC@.p r21,r22
	@OC@.mi r24,r25
	@OC@.n r27,r28
	@OC@.cs r30,r31
	@OC@.c r33,r34
	@OC@.lo r36,r37
	@OC@.cc r39,r40
	@OC@.nc r42,r43
	@OC@.hs r45,r46
	@OC@.vs r48,r49
	@OC@.v r51,r52
	@OC@.vc r54,r55
	@OC@.nv r57,r58
	@OC@.gt r60,r60
	@OC@.ge r0,0
	@OC@.lt 1,r1
	@OC@.le 2,2
	@OC@.hi r3,r3
	@OC@.ls r4,r4
	@OC@.pnz r5,r5

# flag setting
	@OC@.f r0,r1
	@OC@.f r2,1
	@OC@.f 1,r3
	@OC@.f 0,r4
	@OC@.f r5,512
	@OC@.f 512,r6
	@OC@.f 512,512

# conditional execution + flag setting
	@OC@.eq.f r0,r1
	@OC@.ne.f r1,0
	@OC@.lt.f 0,r2
	@OC@.gt.f 1,r2
	@OC@.le.f r0,512
	@OC@.ge.f 512,r2
	@OC@.n.f 512,512
