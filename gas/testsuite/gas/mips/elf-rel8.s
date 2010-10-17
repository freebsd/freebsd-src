	.equ	$fprel, 2

	.ent	foo
foo:
	# Test various forms of relocation syntax.

	lui	$4,(%hi gvar)
	addiu	$4,$4,(%lo (gvar))
	lw	$4,%lo gvar($5)

	# Check that registers aren't confused with $ identifiers.

	lw	$4,($fprel)($fp)

	# Check various forms of paired relocations.

	lui	$4,%call_hi(gfunc)
	addu	$4,$4,$gp
	lw	$25,%call_lo(gfunc)($4)

	lui	$4,%got_hi(gvar)
	addu	$4,$4,$gp
	lw	$5,%got_lo(gvar)($4)

	lw	$4,%got(lvar)($28)
	sb	$5,%lo(lvar)($4)

	lui	$4,%call_hi(gfunc)
	addiu	$4,$4,%call_lo(gfunc)

	lui	$4,%got_hi(gvar)
	addiu	$4,$4,%got_lo(gvar)

	lw	$4,%got(lvar)($28)
	addiu	$4,$4,%lo(lvar)

	# Check individual relocations.

	lw	$25,%call16(gfunc)($28)
	addiu	$4,$28,%call16(gfunc)

	lw	$4,%got_disp(gvar)($28)
	addiu	$4,$28,%got_disp(gvar)

	lw	$4,%gp_rel(gvar)($28)
	sw	$4,%gp_rel(gvar)($28)
	addiu	$4,$28,%gp_rel(gvar)

	.space	64
	.end	foo

	.data
lvar:	.word	1,2
