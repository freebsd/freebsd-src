	.globl		B
	.globl		C

	.section	.foo,"awx",%progbits
	.4byte		1,2,3,4
B:
	.4byte		5,6,7

	.section	.bar,"ax",%nobits
	.space		0x123
C:
	.space		0x302

	.globl		D
	.equ		D,0x12345678
