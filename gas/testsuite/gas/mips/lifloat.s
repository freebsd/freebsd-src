# Source file used to test the li.d and li.s macros.
	
foo:	
	li.d	$4,1.12345
	li.d	$f4,1.12345
	
	li.s	$4,1.12345
	li.s	$f4,1.12345

# Round to a 16 byte boundary, for ease in testing multiple targets.
	.ifdef	SVR4
	nop
	nop
	nop
	.endif
	.ifdef	XGOT
	nop
	nop
	nop
	.endif
