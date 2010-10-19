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

	.data
	.word	undef
