# Insn 3 @OC@ test

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
	@OC@.eq r10,r11
	@OC@.ne r12,r13
	@OC@.lt r14,0
	@OC@.gt r15,512

# flag setting
	@OC@.f r0,r1
	@OC@.f r2,1
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
