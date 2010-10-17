	.text
	.global foo
foo:
	movw $bar-foo,%si

	# Force a good alignment.
	.p2align        4,0

	.data
bar:
	.long	0
