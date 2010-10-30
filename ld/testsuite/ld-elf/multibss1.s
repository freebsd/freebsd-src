	.macro	makebss
	.section .bss_\@,"aw",%nobits
	.space	0x10000
	.endm

	.rept	80
	makebss
	.endr

	.text
	.space	0x10
