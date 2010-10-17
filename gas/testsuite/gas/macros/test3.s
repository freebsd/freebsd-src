	.macro	m arg1 arg2
	\arg1
	.exitm
	\arg2
	.endm

	m	".long foo1",.garbage
