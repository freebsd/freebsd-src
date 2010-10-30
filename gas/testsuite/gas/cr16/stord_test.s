        .text
        .global main
main:
	######################
	#  stord abs20/24 regp
	######################
	stord (r1,r0),0x0
	stord (r1,r0),0xff
	stord (r3,r2),0xfff
	stord (r4,r3),0x1234
	stord (r5,r4),0x1234
	stord (r1,r0),0x7A1234
	stord (r1,r0),0xBA1234
	stord (r2,r1),0xffffff
	######################
	#  stord abs20 rel regp
	######################
	stord (r1,r0),[r12]0x0
	stord (r1,r0),[r13]0x0
	stord (r1,r0),[r12]0xff
	stord (r1,r0),[r13]0xff
	stord (r3,r2),[r12]0xfff
	stord (r3,r2),[r13]0xfff
	stord (r4,r3),[r12]0x1234
	stord (r4,r3),[r13]0x1234
	stord (r5,r4),[r12]0x1234
	stord (r5,r4),[r13]0x1234
	stord (r2,r1),[r12]0x4567
	stord (r2,r1),[r13]0xA1234
	###################################
	#  stord regp rbase(disp20/-disp20)  
	###################################
	stord (r2,r1),0x4(r1,r0)
	stord (r3,r2),0x4(r3,r2)
	stord (r4,r3),0x1234(r1,r0)
	stord (r5,r4),0x1234(r3,r2)
	stord (r6,r5),0xA1234(r1,r0)
	stord (r2,r1),-0x4(r1,r0)
	stord (r3,r2),-0x4(r3,r2)
	stord (r4,r3),-0x1234(r1,r0)
	stord (r5,r4),-0x1234(r3,r2)
	stord (r6,r5),-0xA1234(r1,r0)
	#################################################
	#  stord regp rpbase(disp4/disp16/disp20/-disp20) 
	#################################################
	stord (r1,r0),0x0(r1,r0)
	stord (r1,r0),0x0(r1,r0)
	stord (r1,r0),0xf(r1,r0)
	stord (r1,r0),0xf(r1,r0)
	stord (r2,r1),0x1234(r1,r0)
	stord (r3,r2),0xabcd(r3,r2)
	stord (r4,r3),0xAfff(r4,r3)
	stord (r7,r6),0xA1234(r6,r5)
	stord (r1,r0),-0xf(r1,r0)
	stord (r1,r0),-0xf(r1,r0)
	stord (r2,r1),-0x1234(r1,r0)
	stord (r3,r2),-0xabcd(r3,r2)
	stord (r5,r4),-0xAfff(r4,r3)
	stord (r5,r4),-0xA1234(r6,r5)
	####################################
	#  stord rbase(disp0/disp14) rel reg
	####################################
	stord (r1,r0),[r12]0x0(r1,r0)
	stord (r1,r0),[r13]0x0(r1,r0)
	stord (r2,r1),[r12]0x1234(r1,r0)
	stord (r3,r2),[r13]0x1abcd(r1,r0)
	#################################
	#  stord rpbase(disp20) rel reg
	#################################
	stord (r3,r2),[r12]0xA1234(r1,r0)
	stord (r4,r3),[r13]0xB1234(r1,r0)
	stord (r5,r4),[r13]0xfffff(r1,r0)
