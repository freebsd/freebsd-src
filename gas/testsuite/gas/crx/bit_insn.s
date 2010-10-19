# Bit instructions.
 .data
foodata: .word 42
	 .text
footext:

# cbit instructions.
	.global cbitb
cbitb:
cbitb $6, 0x450
cbitb $0x6, 0xffff0450
cbitb $7, 0x41287
cbitb $3, 9(r5)
cbitb $0, (sp)
cbitb $2, 0xffffe1(r1)
cbitb $4, 0xfa(ra,sp,1)
cbitb $0x7, -333(r15,r7,8)

	.global cbitw
cbitw:
cbitw $0xf, 0x23
cbitw $0x6, 0xffff0023
cbitw $01, 0xff287
cbitw $15, 1(r5)
cbitw $0, (r14)
cbitw $5, 0xffffe1(r1)
cbitw $8, 0xaf(ra,sp,2)
cbitw $0x7, -200(r1,r3,4)

	.global cbitd
cbitd:
cbitd $6, 0xff
cbitd $0x6, 0xffff0fff
cbitd $0x1a, 0x10000
cbitd $31, 7(r9)
cbitd $020, (r2)
cbitd $26, 0xffffe1(r2)
cbitd $30, 0xa(r3,sp,1)
cbitd $0x7, -0x480(r4,r5,8)
cbitd r6, r8
cbitd $30, r4

# sbit instructions.
	.global sbitb
sbitb:
sbitb $6, 0x450
sbitb $0x6, 0xffff0450
sbitb $7, 0x41287
sbitb $3, 9(r5)
sbitb $0, (sp)
sbitb $2, 0xffffe1(r1)
sbitb $4, 0xfa(ra,sp,1)
sbitb $0x7, -333(r15,r7,8)

	.global sbitw
sbitw:
sbitw $0xf, 0x23
sbitw $0x6, 0xffff0023
sbitw $01, 0xff287
sbitw $15, 1(r5)
sbitw $0, (r14)
sbitw $5, 0xffffe1(r1)
sbitw $8, 0xaf(ra,sp,2)
sbitw $0x7, -200(r1,r3,4)

	.global sbitd
sbitd:
sbitd $6, 0xff
sbitd $0x6, 0xffff0fff
sbitd $0x1a, 0x10000
sbitd $31, 7(r9)
sbitd $020, (r2)
sbitd $26, 0xffffe1(r2)
sbitd $30, 0xa(r3,sp,1)
sbitd $0x7, -0x480(r4,r5,8)
sbitd r6, r8
sbitd $30, r4

# tbit instructions.
	.global tbitb
tbitb:
tbitb $6, 0x450
tbitb $0x6, 0xffff0450
tbitb $7, 0x41287
tbitb $3, 9(r5)
tbitb $0, (sp)
tbitb $2, 0xffffe1(r1)
tbitb $4, 0xfa(ra,sp,1)
tbitb $0x7, -333(r15,r7,8)

	.global tbitw
tbitw:
tbitw $0xf, 0x23
tbitw $0x6, 0xffff0023
tbitw $01, 0xff287
tbitw $15, 1(r5)
tbitw $0, (r14)
tbitw $5, 0xffffe1(r1)
tbitw $8, 0xaf(ra,sp,2)
tbitw $0x7, -200(r1,r3,4)

	.global tbitd
tbitd:
tbitd $6, 0xff
tbitd $0x6, 0xffff0fff
tbitd $0x1a, 0x10000
tbitd $31, 7(r9)
tbitd $020, (r2)
tbitd $26, 0xffffe1(r2)
tbitd $30, 0xa(r3,sp,1)
tbitd $0x7, -0x480(r4,r5,8)
tbitd r6, r8
tbitd $30, r4
