	.rept	0x3e0
	nop
	.endr
	ld	$4,foo+8
	.space	16
	.section .rodata
foo:
	.word	1,2,3,4
