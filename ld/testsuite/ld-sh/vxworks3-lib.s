	.macro	entry
	.globl	foo\@
	.size	foo\@,4
	.type	foo\@,@function
foo\@:
	rts
	nop
	.endm
	
	.rept	511
	entry
	.endr
