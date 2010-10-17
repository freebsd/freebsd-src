	.h8300s
	.text
h8300s_bit_ops_1:
	band #0,r0l
	band #0,@er0
	band #0,@64:8
	band #0,@128:16
	band #0,@65536:32
	bclr #0,r0l
	bclr #0,@er0
	bclr #0,@64:8
	bclr #0,@128:16
	bclr #0,@65536:32
	bclr r1l,r0l
	bclr r1l,@er0
	bclr r1l,@64:8
	bclr r1l,@128:16
	bclr r1l,@65536:32
	biand #0,r0l
	biand #0,@er0
	biand #0,@64:8
	biand #0,@128:16
	biand #0,@65536:32
	bild #0,r0l
	bild #0,@er0
	bild #0,@64:8
	bild #0,@128:16
	bild #0,@65536:32

