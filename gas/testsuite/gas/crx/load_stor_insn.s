# Load/Store instructions.
 .data
foodata: .word 42
	 .text
footext:

# Load instructions (memory to register).
	.global loadb
loadb:
loadb 0x632, r1
loadb 0x87632, r2
loadb 0xffff1234, r3
loadb 9(r5), r4
loadb 0(sp), r6
loadb 0x456(r6), r7
loadb -0x456(r8), r10
loadb 0x45678(r13), r12
loadb -0x4567892(r9), sp
loadb 0x9(sp)+, ra
loadb -34(r13)+, r2
loadb 0x45(r9,r12,2), r13
loadb -657(r15,r7,1), r14

	.global loadw
loadw:
loadw 0632, r1
loadw 87632, r2
loadw 0xffff0006, r3
loadw 2(r15), r4
loadw 0(sp), r6
loadw 0456(r6), r7
loadw -0x7ff(r8), r10
loadw 456789(r13), r12
loadw -16777216(r9), sp
loadw 010(r2)+, ra
loadw -0x34(r13)+, r2
loadw 045(r9,r12,4), r13
loadw -0x6657(r15,r7,8), r14

	.global loadd
loadd:
loadd 0xfff1, r1
loadd 0xffefffef, r2
loadd 0xffff1234, r3
loadd 10(r0), r4
loadd 0(sp), r6
loadd 0x100(r6), r7
loadd -0x100(r8), r10
loadd 0220000(r13), r12
loadd -014400000(r9), sp
loadd 07(sp)+, ra
loadd -50(ra)+, r2
loadd 45(r9,r12,2), r13
loadd -0657(r15,r7,1), r14

# Store instructions (register/immediate to memory).
	.global storb
storb:
storb r1, 0x632
storb r2, 0x87632
storb r3, 0xffff1234
storb r4, 9(r5)
storb r6, 0(sp)
storb r7, 0x456(r6)
storb r10, -0x456(r8)
storb r12, 0x45678(r13)
storb sp, -0x4567892(r9)
storb ra, 0x9(sp)+
storb r2, -34(r13)+
storb r13, 0x45(r9,r12,2)
storb r14, -657(r15,r7,1)
storb $5, 9(r4)
storb $15, -0xfed(r3)

	.global storw
storw:
storw r1, 0632
storw r2, 87632
storw r3, 0xffff0006
storw r4, 2(r15)
storw r6, 0(sp)
storw r7, 0456(r6)
storw r10, -0x7ff(r8)
storw r12, 456789(r13)
storw sp, -16777216(r9)
storw ra, 010(r2)+
storw r2, -0x34(r13)+
storw r13, 045(r9,r12,4)
storw r14, -0x6657(r15,r7,8)
storw $01, 0x632
storw $0x7, 0x87632

	.global stord
stord:
stord r1, 0xfff1
stord r2, 0xffefffef
stord r3, 0xffff0001
stord r4, 10(r0)
stord r6, 0(sp)
stord r7, 0x100(r6)
stord r10, -0x100(r8)
stord r12, 0220000(r13)
stord sp, -014400000(r9)
stord ra, 07(sp)+
stord r2, -50(ra)+
stord r13, 45(r9,r12,2)
stord r14, -0657(r15,r7,1)
stord $0xf, 05(r10)+
stord $0x0, -034(r11)+

