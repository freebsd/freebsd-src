	.macro	load
	.text
	lw	$4,%gottprel(foo\@)($gp)

	.global foo\@
	.type   foo\@,@object
	.size   foo\@,4
	.section .tdata,"awT",@progbits
foo\@:
	.word   \@
	.endm

	.rept	4
	load
	.endr

	.text
	.macro	load2
	lw	$4,%got(undefb\@)($gp)
	.endm

	.rept	0x3000
	load2
	.endr

	.data
	.word	undef
