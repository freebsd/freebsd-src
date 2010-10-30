# All-true IT block macro.

	.macro itblock num cond=""
	.if x\cond != x
	.if \num == 4
	itttt \cond
	.else
	.if \num == 3
	ittt \cond
	.else
	.if \num == 2
	itt \cond
	.else
	.if \num == 1
	.it \cond
	.endif
	.endif
	.endif
	.endif
	.endif
	.endm
