        .text
        .global main
main:
	######################
	#  storw reg abs20/24 
	######################
	storw r0,0x0
	storw r1,0xff
	storw r3,0xfff
	storw r4,0x1234
	storw r5,0x1234
	storw r0,0x7A1234
	storw r1,0xBA1234
	storw r2,0xffffff
	######################
	#  storw abs20 rel reg
	######################
	storw r0,[r12]0x0
	storw r0,[r13]0x0
	storw r1,[r12]0xff
	storw r1,[r13]0xff
	storw r3,[r12]0xfff
	storw r3,[r13]0xfff
	storw r4,[r12]0x1234
	storw r4,[r13]0x1234
	storw r5,[r12]0x1234
	storw r5,[r13]0x1234
	storw r2,[r12]0x4567
	storw r2,[r13]0xA1234
	###################################
	#  storw reg rbase(disp20/-disp20) 
	###################################
	storw r1,0x4(r1,r0)
	storw r3,0x4(r3,r2)
	storw r4,0x1234(r1,r0)
	storw r5,0x1234(r3,r2)
	storw r6,0xA1234(r1,r0)
	storw r1,-0x4(r1,r0)
	storw r3,-0x4(r3,r2)
	storw r4,-0x1234(r1,r0)
	storw r5,-0x1234(r3,r2)
	storw r6,-0xA1234(r1,r0)
	#################################################
	#  storw reg rpbase(disp4/disp16/disp20/-disp20) 
	#################################################
	storw r0,0x0(r1,r0)
	storw r0,0x0(r1,r0)
	storw r0,0xf(r1,r0)
	storw r1,0xf(r1,r0)
	storw r2,0x1234(r1,r0)
	storw r3,0xabcd(r3,r2)
	storw r4,0xAfff(r4,r3)
	storw r5,0xA1234(r6,r5)
	storw r0,-0xf(r1,r0)
	storw r1,-0xf(r1,r0)
	storw r2,-0x1234(r1,r0)
	storw r3,-0xabcd(r3,r2)
	storw r4,-0xAfff(r4,r3)
	storw r5,-0xA1234(r6,r5)
	####################################
	#  storw rbase(disp0/disp14) rel reg
	####################################
	storw r0,[r12]0x0(r1,r0)
	storw r1,[r13]0x0(r1,r0)
	storw r2,[r12]0x1234(r1,r0)
	storw r3,[r13]0x1abcd(r1,r0)
	#################################
	#  storw reg rpbase(disp20) rel
	#################################
	storw r4,[r12]0xA1234(r1,r0)
	storw r5,[r13]0xB1234(r1,r0)
	storw r6,[r13]0xfffff(r1,r0)
	#######################
	# storw reg, uimm16/20
	######################
	storw $4,0xbcd
	storw $5,0xaabcd
	storw $3,0xfaabcd

	#######################
	# storw reg, uimm16/20
	######################
	storw $5,[r12]0x14
	storw $4,[r13]0xabfc
	storw $3,[r12]0x1234
	storw $3,[r13]0x1234
	storw $3,[r12]0x34
	#######################
	# storw imm, index-rbase
	######################
	storw $3,[r12]0xa7a(r1,r0)
	storw $3,[r12]0xa7a(r3,r2)
	storw $3,[r12]0xa7a(r4,r3)
	storw $3,[r12]0xa7a(r5,r4)
	storw $3,[r12]0xa7a(r6,r5)
	storw $3,[r12]0xa7a(r7,r6)
	storw $3,[r12]0xa7a(r9,r8)
	storw $3,[r12]0xa7a(r11,r10)
	storw $3,[r13]0xa7a(r1,r0)
	storw $3,[r13]0xa7a(r3,r2)
	storw $3,[r13]0xa7a(r4,r3)
	storw $3,[r13]0xa7a(r5,r4)
	storw $3,[r13]0xa7a(r6,r5)
	storw $3,[r13]0xa7a(r7,r6)
	storw $3,[r13]0xa7a(r9,r8)
	storw $3,[r13]0xa7a(r11,r10)
	storw $5,[r13]0xb7a(r4,r3)
	storw $1,[r12]0x17a(r6,r5)
	storw $1,[r13]0x134(r6,r5)
	storw $3,[r12]0xabcde(r4,r3)
	storw $5,[r13]0xabcd(r4,r3)
	storw $3,[r12]0xabcd(r6,r5)
	storw $3,[r13]0xbcde(r6,r5)
	#######################
	# storw imm4, rbase(disp)
	######################
	storw $5,0x0(r2)
	storw $3,0x34(r12)
	storw $3,0xab(r13)
	storw $5,0xad(r1)
	storw $5,0xcd(r2)
	storw $5,0xfff(r0)
	storw $3,0xbcd(r4)
	storw $3,0xfff(r12)
	storw $3,0xfff(r13)
	storw $3,0xffff(r13)
	storw $3,0x2343(r12)
	storw $3,0x12345(r2)
	storw $3,0x4abcd(r8)
	storw $3,0xfabcd(r13)
	storw $3,0xfabcd(r8)
	storw $3,0xfabcd(r9)
	storw $3,0x4abcd(r9)
	##########################
	# storw imm, disp20(rpbase)
	#########################
	storw $3,0x0(r2,r1)
	storw $5,0x1(r2,r1)
	storw $4,0x1234(r2,r1)
	storw $3,0x1234(r2,r1)
	storw $3,0x12345(r2,r1)
	storw $3,0x123(r2,r1)
	storw $3,0x12345(r2,r1)

