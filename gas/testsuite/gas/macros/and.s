	TFLAG_C = 1

	.macro	check
	.if	(0 & TFLAG_C)
	.endif
	.endm

	.text
	check
