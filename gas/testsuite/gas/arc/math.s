# @OC@ test

# Stay away from operands with duplicate arguments (eg:	add r0,r1,r1).
# They will be disassembled as they're macro counterparts (eg: asl r0,r1).

# reg,reg,reg
	@OC@ r0,r1,r2
	@OC@ r26,fp,sp
	@OC@ ilink1,ilink2,blink
	@OC@ r58,r59,lp_count

# shimm values
	@OC@ r0,r1,0
	@OC@ r0,0,r2
	@OC@ 0,r1,r2
	@OC@ r0,r1,-1
	@OC@ r0,-1,r2
	@OC@ -1,r1,r2
	@OC@ r0,r1,255
	@OC@ r0,255,r2
	@OC@ 255,r1,r2
	@OC@ r0,r1,-256
	@OC@ r0,-256,r2
	@OC@ -256,r1,r2

# limm values
	@OC@ r0,r1,256
	@OC@ r0,-257,r2
	@OC@ 511,r1,r2
	@OC@ r0,0x42424242,r2
	@OC@ 0x12345678,r1,0x12345678

# shimm and limm
	@OC@ r0,255,256
	@OC@ r0,256,255
	@OC@ 255,r1,256
	@OC@ 255,256,r2
	@OC@ 256,r1,255
	@OC@ 256,255,r2

# symbols
	@OC@ r0,r1,foo

# conditional execution
	@OC@.al r0,r1,r2
	@OC@.ra r3,r4,r5
	@OC@.eq r6,r7,r8
	@OC@.z r9,r10,r11
	@OC@.ne r12,r13,r14
	@OC@.nz r15,r16,r17
	@OC@.pl r18,r19,r20
	@OC@.p r21,r22,r23
	@OC@.mi r24,r25,r26
	@OC@.n r27,r28,r29
	@OC@.cs r30,r31,r32
	@OC@.c r33,r34,r35
	@OC@.lo r36,r37,r38
	@OC@.cc r39,r40,r41
	@OC@.nc r42,r43,r44
	@OC@.hs r45,r46,r47
	@OC@.vs r48,r49,r50
	@OC@.v r51,r52,r53
	@OC@.vc r54,r55,r56
	@OC@.nv r57,r58,r59
	@OC@.gt r60,r60,r0
	@OC@.ge r0,r0,0
	@OC@.lt r1,1,r1
	@OC@.le 2,r1,r2
	@OC@.hi r3,3,r3
	@OC@.ls 4,4,r4
	@OC@.pnz 5,r5,5

# flag setting
	@OC@.f r0,r1,r2
	@OC@.f r0,r1,1
	@OC@.f r0,1,r2
	@OC@.f 0,r1,r2
	@OC@.f r0,r1,512
	@OC@.f r0,512,r2
	@OC@.f 512,r1,r2

# conditional execution + flag setting
	@OC@.eq.f r0,r1,r2
	@OC@.ne.f r0,r1,0
	@OC@.lt.f r0,0,r2
	@OC@.gt.f 0,r1,r2
	@OC@.le.f r0,r1,512
	@OC@.ge.f r0,512,r2
	@OC@.n.f 512,r1,r2
