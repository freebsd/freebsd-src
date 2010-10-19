	.global here
	.type	here,@function
here:
	nop
.Lfe3:
	.size	here,.Lfe3-dsofn

	.type	pfn,@function
pfn:
	bsr	expfn
	nop
	bsr	dsofn3
	nop
.Lfe1:
	.size	pfn,.Lfe1-pfn

	.global dsofn
	.type	dsofn,@function
dsofn:
	move.d	expobj,$r10
	nop
.Lfe2:
	.size	dsofn,.Lfe2-dsofn

