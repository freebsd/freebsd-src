	.macro	entry
	.long	foo\@
	.endm
	
	.rept	511
	entry
	.endr
