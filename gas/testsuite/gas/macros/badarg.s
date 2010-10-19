	.macro
	.endm

	.macro	,arg1
	.endm

	.macro	m1,
	.endm

	.macro	m2,,
	.endm

	.macro	m3,arg1,
	.endm

	.macro	m4,,arg2
	.endm

	.macro	m5,arg,arg
	.endm

	.altmacro

	.macro	m6,arg
	 local arg
	.endm

	.macro	m7
	 local arg
	 local arg
	.endm

	m6
	m7

	.noaltmacro

	.macro m8, arg :
	.endm

	.macro m9, arg : qual
	.endm

	.macro m10, arg : req = def
	.endm

	m10

	.macro m11, arg1 : vararg, arg2
	.endm
