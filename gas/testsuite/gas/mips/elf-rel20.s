	lui	$4,%hi(foo)
	lui	$5,%hi(foo + 0x80000)
	lui	$7,%hi(bar + 0x80000)
	lui	$6,%hi(bar)
	addiu	$4,$4,%lo(foo + 2)
	addiu	$5,$5,%lo(foo + 0x80004)
	addiu	$6,$6,%lo(bar + 2)
	addiu	$7,$7,%lo(bar + 0x80004)
	.section .bss
bar:
	.space	0x80010
