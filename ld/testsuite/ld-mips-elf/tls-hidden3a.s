	.macro	load
	lw	$4,%gottprel(foo\@)($gp)
	.endm

	.rept	4
	load
	.endr

	.section .tdata,"awT",@progbits
	.fill	0xabc0
