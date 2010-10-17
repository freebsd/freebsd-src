	.h8300s
	.text
h8300s_bit_ops_2:
	bior #0,r0l
	bior #0,@er0
	bior #0,@64:8
	bior #0,@128:16
	bior #0,@65536:32
	bist #0,r0l
	bist #0,@er0
	bist #0,@64:8
	bist #0,@128:16
	bist #0,@65536:32
	bixor #0,r0l
	bixor #0,@er0
	bixor #0,@64:8
	bixor #0,@128:16
	bixor #0,@65536:32
	bld #0,r0l
	bld #0,@er0
	bld #0,@64:8
	bld #0,@128:16
	bld #0,@65536:32
