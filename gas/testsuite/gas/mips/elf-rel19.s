	.abicalls
	la	$4,L2
	la	$4,L2 + 2
	la	$4,L2 + 0xc000
	.space	16
        .section        .rodata.str1.1,"aMS",@progbits,1
L1:     .string "foo"
L2:     .string "a"
