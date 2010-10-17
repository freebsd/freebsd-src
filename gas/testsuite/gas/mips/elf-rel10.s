	.globl	foo
	.ent	foo
foo:
	lui	$gp,%hi(%neg(%gp_rel(foo)))
	addiu	$gp,$gp,%lo(%neg(%gp_rel(foo)))
	daddu	$gp,$gp,$25
	.end	foo

	.ent	bar
bar:
	lui	$gp,%hi(%neg(%gp_rel(bar)))
	addiu	$gp,$gp,%lo(%neg(%gp_rel(bar)))
	daddu	$gp,$gp,$25
	.end	bar

	.ent	frob
	lw	$4,%got_page(foo)($gp)
	addiu	$4,$4,%got_ofst(foo)

	lw	$4,%got_page(foo + 0x1234)($gp)
	addiu	$4,$4,%got_ofst(foo + 0x1234)

	lw	$4,%got_page(bar)($gp)
	addiu	$4,$4,%got_ofst(bar)

	lw	$4,%got_page(bar + 0x332211)($gp)
	addiu	$4,$4,%got_ofst(bar + 0x332211)

	lw	$4,%got_page(frob)($gp)
	addiu	$4,$4,%got_ofst(frob)
	.end	frob
