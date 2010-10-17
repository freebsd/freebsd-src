	.macro	m arg1 arg2
	.globl	\arg1
	\arg1 = \arg2
	.endm

	m s1,1
	m s2,2
