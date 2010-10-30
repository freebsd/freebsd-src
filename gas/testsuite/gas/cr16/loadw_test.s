        .text
        .global main
main:
	######################
	#  loadw abs20/24 reg
	######################
	loadw 0x0,r0
	loadw 0xff,r1
	loadw 0xfff,r3
	loadw 0x1234,r4
	loadw 0x1234,r5
	loadw 0x7A1234,r0
	loadw 0xBA1234,r1
	loadw 0xffffff,r2
	######################
	#  loadw abs20 rel reg
	######################
	loadw [r12]0x0,r0
	loadw [r13]0x0,r0
	loadw [r12]0xff,r1
	loadw [r13]0xff,r1
	loadw [r12]0xfff,r3
	loadw [r13]0xfff,r3
	loadw [r12]0x1234,r4
	loadw [r13]0x1234,r4
	loadw [r12]0x1234,r5
	loadw [r13]0x1234,r5
	loadw [r12]0x4567,r2
	loadw [r13]0xA1234,r2
	###################################
	#  loadw rbase(disp20/-disp20)  reg
	###################################
	loadw 0x4(r1,r0),r1
	loadw 0x4(r3,r2),r3
	loadw 0x1234(r1,r0),r4
	loadw 0x1234(r3,r2),r5
	loadw 0xA1234(r1,r0),r6
	loadw -0x4(r1,r0),r1
	loadw -0x4(r3,r2),r3
	loadw -0x1234(r1,r0),r4
	loadw -0x1234(r3,r2),r5
	loadw -0xA1234(r1,r0),r6
	#################################################
	#  loadw rpbase(disp4/disp16/disp20/-disp20)  reg
	#################################################
	loadw 0x0(r1,r0),r0
	loadw 0x0(r1,r0),r1
	loadw 0xf(r1,r0),r0
	loadw 0xf(r1,r0),r1
	loadw 0x1234(r1,r0),r2
	loadw 0xabcd(r3,r2),r3
	loadw 0xAfff(r4,r3),r4
	loadw 0xA1234(r6,r5),r5
	loadw -0xf(r1,r0),r0
	loadw -0xf(r1,r0),r1
	loadw -0x1234(r1,r0),r2
	loadw -0xabcd(r3,r2),r3
	loadw -0xAfff(r4,r3),r4
	loadw -0xA1234(r6,r5),r5
	####################################
	#  loadw rbase(disp0/disp14) rel reg
	####################################
	loadw [r12]0x0(r1,r0),r0
	loadw [r13]0x0(r1,r0),r1
	loadw [r12]0x1234(r1,r0),r2
	loadw [r13]0x1abcd(r1,r0),r3
	#################################
	#  loadw rpbase(disp20) rel reg
	#################################
	loadw [r12]0xA1234(r1,r0),r4
	loadw [r13]0xB1234(r1,r0),r5
	loadw [r13]0xfffff(r1,r0),r6
