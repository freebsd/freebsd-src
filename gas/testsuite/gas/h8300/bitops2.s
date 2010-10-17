	.text
h8300_bit_ops_2:
	bior #0,r0l
	bior #0,@r0
	bior #0,@64:8
	bist #0,r0l
	bist #0,@r0
	bist #0,@64:8
	bixor #0,r0l
	bixor #0,@r0
	bixor #0,@64:8
	bld #0,r0l
	bld #0,@r0
	bld #0,@64:8

