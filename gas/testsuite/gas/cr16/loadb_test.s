        .text
        .global main
main:
	######################
	#  loadb abs20/24 reg
	######################
	loadb 0x0,r0
	loadb 0xff,r1
	loadb 0xfff,r3
	loadb 0x1234,r4
	loadb 0x1234,r5
	loadb 0x7A1234,r0
	loadb 0xBA1234,r1
	loadb 0xffffff,r2
	######################
	#  loadb abs20 rel reg
	######################
	loadb [r12]0x0,r0
	loadb [r13]0x0,r0
	loadb [r12]0xff,r1
	loadb [r13]0xff,r1
	loadb [r12]0xfff,r3
	loadb [r13]0xfff,r3
	loadb [r12]0x1234,r4
	loadb [r13]0x1234,r4
	loadb [r12]0x1234,r5
	loadb [r13]0x1234,r5
	loadb [r12]0x4567,r2
	loadb [r13]0xA1234,r2
	###################################
	#  loadb rbase(disp20/-disp20)  reg
	###################################
	loadb 0x4(r1,r0),r1
	loadb 0x4(r3,r2),r3
	loadb 0x1234(r1,r0),r4
	loadb 0x1234(r3,r2),r5
	loadb 0xA1234(r1,r0),r6
	loadb -0x4(r1,r0),r1
	loadb -0x4(r3,r2),r3
	loadb -0x1234(r1,r0),r4
	loadb -0x1234(r3,r2),r5
	loadb -0xA1234(r1,r0),r6
	#################################################
	#  loadb rpbase(disp4/disp16/disp20/-disp20)  reg
	#################################################
	loadb 0x0(r1,r0),r0
	loadb 0x0(r1,r0),r1
	loadb 0xf(r1,r0),r0
	loadb 0xf(r1,r0),r1
	loadb 0x1234(r1,r0),r2
	loadb 0xabcd(r3,r2),r3
	loadb 0xAfff(r4,r3),r4
	loadb 0xA1234(r6,r5),r5
	loadb -0xf(r1,r0),r0
	loadb -0xf(r1,r0),r1
	loadb -0x1234(r1,r0),r2
	loadb -0xabcd(r3,r2),r3
	loadb -0xAfff(r4,r3),r4
	loadb -0xA1234(r6,r5),r5
	####################################
	#  loadb rbase(disp0/disp14) rel reg
	####################################
	loadb [r12]0x0(r1,r0),r0
	loadb [r13]0x0(r1,r0),r1
	loadb [r12]0x1234(r1,r0),r2
	loadb [r13]0x1abcd(r1,r0),r3
	#################################
	#  loadb rpbase(disp20) rel reg
	#################################
	loadb [r12]0xA1234(r1,r0),r4
	loadb [r13]0xB1234(r1,r0),r5
	loadb [r13]0xfffff(r1,r0),r6
