	.ent	foo
foo:
	lw	$4,%call16(bar)($28)
	la	$5,.L1
	# Insert an instruction that doesn't use $5 to avoid a spurious
	# nop after the previous load macro.
	addiu	$6,$6,1
.L1:
	.space	32
	.end	foo
