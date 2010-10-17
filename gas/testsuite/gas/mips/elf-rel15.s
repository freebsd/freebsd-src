foo:
	lui	$2,%hi(%neg(%gp_rel(foo)))
	sub	$sp,$sp,28
	.space	16
