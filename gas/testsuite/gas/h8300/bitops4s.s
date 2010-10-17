	.h8300s
	.text
h8300s_bit_ops_4:
	bor #0,r0l
	bor #0,@er0
	bor #0,@64:8
	bor #0,@128:16
	bor #0,@65536:32
	bst #0,r0l
	bst #0,@er0
	bst #0,@64:8
	bst #0,@128:16
	bst #0,@65536:32
	btst #0,r0l
	btst #0,@er0
	btst #0,@64:8
	btst #0,@128:16
	btst #0,@65536:32
	btst r1l,r0l
	btst r1l,@er0
	btst r1l,@64:8
	btst r1l,@128:16
	btst r1l,@65536:32
	bxor #0,r0l
	bxor #0,@er0
	bxor #0,@64:8
	bxor #0,@128:16
	bxor #0,@65536:32

