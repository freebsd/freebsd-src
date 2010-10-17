	.ent	foo
foo:
	lw	$4,%got(l1)($gp)
	lw	$4,%got(l2)($gp)
	lw	$4,%got(l3)($gp)
	lw	$4,%got(l3)($gp)
	lw	$4,%got(l1+0x400)($gp)
	addiu	$4,$4,%lo(l1)
	addiu	$4,$4,%lo(l1+0x400)
	addiu	$4,$4,%lo(l3)
	addiu	$4,$4,%lo(l2)
	.space	64
	.end	foo

	.data
l1:	.word	1

	.lcomm	l2, 4

	.rdata
	.word	1
l3:	.word	2
