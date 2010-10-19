# Arithmetic instructions.
 .data
foodata: .word 42
	 .text
footext:

	.global addub
addub:
addub $0x0 , r1
addub $0x5 , r2
addub r3 , r4

	.global addb
addb:
addb $0x1 , r5
addb $0x6 , r6
addb r7 , r8

	.global addcb
addcb:
addcb $2 , r9
addcb $0x9 , r10
addcb r11 , r12

	.global andb
andb:
andb $0x3 , r13
andb $0x10 , r14
andb r15 , ra

	.global cmpb
cmpb:
cmpb $0x4 , sp
cmpb $0x11 , r1
cmpb r2 , r3

	.global movb
movb:
movb $-4 , r4
movb $0x236 , r5
movb r6 , r7

	.global orb
orb:
orb $-0x1 , r8
orb $0x6980 , r9
orb r10 , r11

	.global subb
subb:
subb $07 , r12
subb $0x7fff , r13
subb r14 , r15

	.global subcb
subcb:
subcb $010 , ra
subcb $-0x56 , sp
subcb r1 , r2

	.global xorb
xorb:
xorb $0x16 , r3
xorb $-0x6ffe , r4
xorb r5 , r6

	.global mulb
mulb:
mulb $0x32 , r7
mulb $0xefa , r8
mulb r9 , r10

	.global adduw
adduw:
adduw $0x20 , r11
adduw $32767 , r12
adduw r13 , r14

	.global addw
addw:
addw $0x12 , r15
addw $-32767 , ra
addw sp , r1

	.global addcw
addcw:
addcw $0x48 , r2
addcw $27 , r3
addcw r4 , r5

	.global andw
andw:
andw $0 , r6
andw $-27 , r7
andw r8 , r9

	.global cmpw
cmpw:
cmpw $1 , r10
cmpw $0x11 , r11
cmpw r12 , r13

	.global movw
movw:
movw $0x2 , r14
movw $07000 , r15
movw ra , sp

	.global orw
orw:
orw $0x3 , r1
orw $-2 , r2
orw r3 , r4

	.global subw
subw:
subw $04 , r5
subw $022 , r6
subw r7 , r8

	.global subcw
subcw:
subcw $-0x4 , r9
subcw $-9 , r10
subcw r11 , r12

	.global xorw
xorw:
xorw $-1 , r13
xorw $0x21 , r14
xorw r15 , ra

	.global mulw
mulw:
mulw $0x7 , sp
mulw $027 , r1
mulw r2 , r3

	.global addud
addud:
addud $0x0 , r1
addud $0x5 , r2
addud $0x55555 , r2
addud r3 , r4

	.global addd
addd:
addd $0x1 , r5
addd $0x6 , r6
addd $0x7fffffff , r6
addd r7 , r8

	.global addcd
addcd:
addcd $2 , r9
addcd $0x9 , r10
addcd $-0x7fffffff , r10
addcd r11 , r12

	.global andd
andd:
andd $0x3 , r13
andd $0x10 , r14
andd $0xffffffff , r14
andd r15 , ra

	.global cmpd
cmpd:
cmpd $0x4 , sp
cmpd $0x11 , r1
cmpd $0xf0000001 , r1
cmpd r2 , r3

	.global movd
movd:
movd $-4 , r4
movd $0x236 , r5
movd $-0x80000000 , r5
movd r6 , r7

	.global ord
ord:
ord $-0x1 , r8
ord $0x6980 , r9
ord $0x10000 , r9
ord r10 , r11

	.global subd
subd:
subd $07 , r12
subd $0x7fff , r13
subd $-0x10000 , r13
subd r14 , r15

	.global subcd
subcd:
subcd $010 , ra
subcd $-0x56 , sp
subcd $4294967295 , sp
subcd r1 , r2

	.global xord
xord:
xord $0x16 , r3
xord $-0x6ffe , r4
xord $017777777777 , r4
xord r5 , r6

	.global muld
muld:
muld $0x32 , r7
muld $0xefa , r8
muld $-017777777777 , r8
muld r9 , r10
