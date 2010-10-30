        .text
        .global main
main:
	######################
	#  storb reg abs20/24 
	######################
	storb r0,0x0
	storb r1,0xff
	storb r3,0xfff
	storb r4,0x1234
	storb r5,0x1234
	storb r0,0x7A1234
	storb r1,0xBA1234
	storb r2,0xffffff
	######################
	#  storb abs20 rel reg
	######################
	storb r0,[r12]0x0
	storb r0,[r13]0x0
	storb r1,[r12]0xff
	storb r1,[r13]0xff
	storb r3,[r12]0xfff
	storb r3,[r13]0xfff
	storb r4,[r12]0x1234
	storb r4,[r13]0x1234
	storb r5,[r12]0x1234
	storb r5,[r13]0x1234
	storb r2,[r12]0x4567
	storb r2,[r13]0xA1234
	###################################
	#  storb reg rbase(disp20/-disp20) 
	###################################
	storb r1,0x4(r1,r0)
	storb r3,0x4(r3,r2)
	storb r4,0x1234(r1,r0)
	storb r5,0x1234(r3,r2)
	storb r6,0xA1234(r1,r0)
	storb r1,-0x4(r1,r0)
	storb r3,-0x4(r3,r2)
	storb r4,-0x1234(r1,r0)
	storb r5,-0x1234(r3,r2)
	storb r6,-0xA1234(r1,r0)
	#################################################
	#  storb reg rpbase(disp4/disp16/disp20/-disp20) 
	#################################################
	storb r0,0x0(r1,r0)
	storb r0,0x0(r1,r0)
	storb r0,0xf(r1,r0)
	storb r1,0xf(r1,r0)
	storb r2,0x1234(r1,r0)
	storb r3,0xabcd(r3,r2)
	storb r4,0xAfff(r4,r3)
	storb r5,0xA1234(r6,r5)
	storb r0,-0xf(r1,r0)
	storb r1,-0xf(r1,r0)
	storb r2,-0x1234(r1,r0)
	storb r3,-0xabcd(r3,r2)
	storb r4,-0xAfff(r4,r3)
	storb r5,-0xA1234(r6,r5)
	####################################
	#  storb rbase(disp0/disp14) rel reg
	####################################
	storb r0,[r12]0x0(r1,r0)
	storb r1,[r13]0x0(r1,r0)
	storb r2,[r12]0x1234(r1,r0)
	storb r3,[r13]0x1abcd(r1,r0)
	#################################
	#  storb reg rpbase(disp20) rel
	#################################
	storb r4,[r12]0xA1234(r1,r0)
	storb r5,[r13]0xB1234(r1,r0)
	storb r6,[r13]0xfffff(r1,r0)
	#######################
	# storb reg, uimm16/20
	######################
	storb $4,0xbcd
	storb $5,0xaabcd
	storb $3,0xfaabcd

	#######################
	# storb reg, uimm16/20
	######################
	storb $5,[r12]0x14
	storb $4,[r13]0xabfc
	storb $3,[r12]0x1234
	storb $3,[r13]0x1234
	storb $3,[r12]0x34
	#######################
	# storb imm, index-rbase
	######################
	storb $3,[r12]0xa7a(r1,r0)
	storb $3,[r12]0xa7a(r3,r2)
	storb $3,[r12]0xa7a(r4,r3)
	storb $3,[r12]0xa7a(r5,r4)
	storb $3,[r12]0xa7a(r6,r5)
	storb $3,[r12]0xa7a(r7,r6)
	storb $3,[r12]0xa7a(r9,r8)
	storb $3,[r12]0xa7a(r11,r10)
	storb $3,[r13]0xa7a(r1,r0)
	storb $3,[r13]0xa7a(r3,r2)
	storb $3,[r13]0xa7a(r4,r3)
	storb $3,[r13]0xa7a(r5,r4)
	storb $3,[r13]0xa7a(r6,r5)
	storb $3,[r13]0xa7a(r7,r6)
	storb $3,[r13]0xa7a(r9,r8)
	storb $3,[r13]0xa7a(r11,r10)
	storb $5,[r13]0xb7a(r4,r3)
	storb $1,[r12]0x17a(r6,r5)
	storb $1,[r13]0x134(r6,r5)
	storb $3,[r12]0xabcde(r4,r3)
	storb $5,[r13]0xabcd(r4,r3)
	storb $3,[r12]0xabcd(r6,r5)
	storb $3,[r13]0xbcde(r6,r5)
	#######################
	# storb imm4, rbase(disp)
	######################
	storb $5,0x0(r2)
	storb $3,0x34(r12)
	storb $3,0xab(r13)
	storb $5,0xad(r1)
	storb $5,0xcd(r2)
	storb $5,0xfff(r0)
	storb $3,0xbcd(r4)
	storb $3,0xfff(r12)
	storb $3,0xfff(r13)
	storb $3,0xffff(r13)
	storb $3,0x2343(r12)
	storb $3,0x12345(r2)
	storb $3,0x4abcd(r8)
	storb $3,0xfabcd(r13)
	storb $3,0xfabcd(r8)
	storb $3,0xfabcd(r9)
	storb $3,0x4abcd(r9)
	##########################
	# storb imm, disp20(rpbase)
	#########################
	storb $3,0x0(r2,r1)
	storb $5,0x1(r2,r1)
	storb $4,0x1234(r2,r1)
	storb $3,0x1234(r2,r1)
	storb $3,0x12345(r2,r1)
	storb $3,0x123(r2,r1)
	storb $3,0x12345(r2,r1)
