#AMDFAM10 New Instructions

	.text
foo:
	lzcnt	(%ecx),%ebx
	lzcnt	(%ecx),%bx
	lzcnt	%ecx,%ebx
	lzcnt	%cx,%bx
	popcnt	(%ecx),%ebx
	popcnt	(%ecx),%bx
	popcnt	%ecx,%ebx
	popcnt	%cx,%bx
	extrq	%xmm2,%xmm1
	extrq	$4,$2,%xmm1
	insertq	%xmm2,%xmm1
	insertq	$4,$2,%xmm2,%xmm1
	movntsd	%xmm1,(%ecx)
	movntss %xmm1,(%ecx)

	# Force a good alignment.
	.p2align	4,0
