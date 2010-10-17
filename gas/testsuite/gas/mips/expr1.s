	.globl	foo
	.ent	foo
foo:
	lw	$4,%lo(foo)($4)
	lw	$4,((10 + 4) * 4)($4)
	lw	$4,%lo (2 * 4) + foo($4)
	lw	$4,%lo((2 * 4) + foo)($4)
	lw	$4,(((%lo ((2 * 4) + foo))))($4)
	.space	64
	.end	foo
