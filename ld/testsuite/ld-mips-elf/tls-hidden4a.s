	.macro	load
	lw	$4,%gottprel(foo\@)($gp)
	.endm

	.rept	4
	load
	.endr

	.macro	load2
	lw	$4,%got(undefa\@)($gp)
	.endm

	.rept	0x3000
	load2
	.endr

	.section .tdata,"awT",@progbits
	.fill	0xabc0
