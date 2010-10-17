	.text
h8300_bit_ops_4:
	bor #0,r0l
	bor #0,@r0
	bor #0,@64:8
	bst #0,r0l
	bst #0,@r0
	bst #0,@64:8
	btst #0,r0l
	btst #0,@r0
	btst #0,@64:8
	btst r1l,r0l
	btst r1l,@r0
	btst r1l,@64:8
	bxor #0,r0l
	bxor #0,@r0
	bxor #0,@64:8

