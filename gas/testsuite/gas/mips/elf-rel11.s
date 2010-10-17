	.ent	foo
foo:
	lui	$4,%highest(bar)
	lui	$5,%hi(bar)
	daddiu	$4,$4,%higher(bar)
	daddiu	$5,$5,%lo(bar)
	dsll32	$4,$4,0
	daddu	$4,$4,$5

	lui	$4,%highest(bar + 0x12345678)
	lui	$5,%hi(bar + 0x12345678)
	daddiu	$4,$4,%higher(bar + 0x12345678)
	daddiu	$5,$5,%lo(bar + 0x12345678)
	dsll32	$4,$4,0
	daddu	$4,$4,$5

	lui	$4,%highest(l1)
	daddiu	$4,$4,%higher(l1)
	dsll	$4,$4,16
	daddiu	$4,$4,%hi(l1)
	dsll	$4,$4,16
	lw	$4,%lo(l1)($4)
	.end	foo

	.data
	.word	1,2,3,4
l1:	.word	4,5
