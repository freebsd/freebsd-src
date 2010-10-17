	.h8300s
	.text
h8300s_bit_ops_3:
	bnot #0,r0l
	bnot #0,@er0
	bnot #0,@64:8
	bnot #0,@128:16
	bnot #0,@65536:32
	bnot r1l,r0l
	bnot r1l,@er0
	bnot r1l,@64:8
	bnot r1l,@128:16
	bnot r1l,@65536:32
	bset #0,r0l
	bset #0,@er0
	bset #0,@64:8
	bset #0,@128:16
	bset #0,@65536:32
	bset r1l,r0l
	bset r1l,@er0
	bset r1l,@64:8
	bset r1l,@128:16
	bset r1l,@65536:32

