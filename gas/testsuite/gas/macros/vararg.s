	.macro	v1 arg1 : req, args : vararg
	.long	foo\arg1
	.ifnb	\args
	v1	\args
	.endif
	.endm

	v1	1
	v1	2, 3
	v1	4, 5, 6
