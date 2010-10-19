	.macro	decl
	.global	exported\@
	.equ	exported\@,\@
	.endm

	.rept	dynsym - base_syms
	decl
	.endr

	lw	$25,%call16(foo)($gp)
