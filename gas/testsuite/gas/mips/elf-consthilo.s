	.set	noreorder
	.set	nomacro
	.set	noat
	.set	nomips16

	.equ	addr, 0xdeadbeef
	.ent	foo
foo:
	lui	$4,%hi(addr)
	jr	$31
	lb	$2,%lo(addr)($4)
	.end	foo
