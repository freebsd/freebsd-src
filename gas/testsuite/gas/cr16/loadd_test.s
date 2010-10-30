        .text
        .global main
main:
	######################
	#  loadd abs20/24 regp
	######################
	loadd 0x0,(r1,r0)
	loadd 0xff,(r1,r0)
	loadd 0xfff,(r3,r2)
	loadd 0x1234,(r4,r3)
	loadd 0x1234,(r5,r4)
	loadd 0x7A1234,(r1,r0)
	loadd 0xBA1234,(r1,r0)
	loadd 0xffffff,(r2,r1)
	######################
	#  loadd abs20 rel regp
	######################
	loadd [r12]0x0,(r1,r0)
	loadd [r13]0x0,(r1,r0)
	loadd [r12]0xff,(r1,r0)
	loadd [r13]0xff,(r1,r0)
	loadd [r12]0xfff,(r3,r2)
	loadd [r13]0xfff,(r3,r2)
	loadd [r12]0x1234,(r4,r3)
	loadd [r13]0x1234,(r4,r3)
	loadd [r12]0x1234,(r5,r4)
	loadd [r13]0x1234,(r5,r4)
	loadd [r12]0x4567,(r2,r1)
	loadd [r13]0xA1234,(r2,r1)
	###################################
	#  loadd rbase(disp20/-disp20)  regp
	###################################
	loadd 0x4(r1,r0),(r2,r1)
	loadd 0x4(r3,r2),(r3,r2)
	loadd 0x1234(r1,r0),(r4,r3)
	loadd 0x1234(r3,r2),(r5,r4)
	loadd 0xA1234(r1,r0),(r6,r5)
	loadd -0x4(r1,r0),(r2,r1)
	loadd -0x4(r3,r2),(r3,r2)
	loadd -0x1234(r1,r0),(r4,r3)
	loadd -0x1234(r3,r2),(r5,r4)
	loadd -0xA1234(r1,r0),(r6,r5)
	#################################################
	#  loadd rpbase(disp4/disp16/disp20/-disp20)  reg
	#################################################
	loadd 0x0(r1,r0),(r1,r0)
	loadd 0x0(r1,r0),(r1,r0)
	loadd 0xf(r1,r0),(r1,r0)
	loadd 0xf(r1,r0),(r1,r0)
	loadd 0x1234(r1,r0),(r2,r1)
	loadd 0xabcd(r3,r2),(r3,r2)
	loadd 0xAfff(r4,r3),(r4,r3)
	loadd 0xA1234(r6,r5),(r7,r6)
	loadd -0xf(r1,r0),(r1,r0)
	loadd -0xf(r1,r0),(r1,r0)
	loadd -0x1234(r1,r0),(r2,r1)
	loadd -0xabcd(r3,r2),(r3,r2)
	loadd -0xAfff(r4,r3),(r5,r4)
	loadd -0xA1234(r6,r5),(r5,r4)
	####################################
	#  loadd rbase(disp0/disp14) rel reg
	####################################
	loadd [r12]0x0(r1,r0),(r1,r0)
	loadd [r13]0x0(r1,r0),(r1,r0)
	loadd [r12]0x1234(r1,r0),(r2,r1)
	loadd [r13]0x1abcd(r1,r0),(r3,r2)
	#################################
	#  loadd rpbase(disp20) rel reg
	#################################
	loadd [r12]0xA1234(r1,r0),(r3,r2)
	loadd [r13]0xB1234(r1,r0),(r4,r3)
	loadd [r13]0xfffff(r1,r0),(r5,r4)
