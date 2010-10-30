#AMDFAM10 New Instructions

	.text
foo:
	lzcnt	(%rcx),%rbx
	lzcnt	(%rcx),%ebx
	lzcnt	(%rcx),%bx
	lzcnt	%rcx,%rbx
	lzcnt	%ecx,%ebx
	lzcnt	%cx,%bx
	popcnt	(%rcx),%rbx
	popcnt	(%rcx),%ebx
	popcnt	(%rcx),%bx
	popcnt	%rcx,%rbx
	popcnt	%ecx,%ebx
	popcnt	%cx,%bx
	extrq	%xmm2,%xmm1
	extrq	$4,$2,%xmm1
	insertq	%xmm2,%xmm1
	insertq	$4,$2,%xmm2,%xmm1
	movntsd	%xmm1,(%rcx)
	movntss %xmm1,(%rcx)

	# Force a good alignment.
	.p2align	4,0
