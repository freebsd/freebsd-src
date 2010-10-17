	.text
h8300_bit_ops_1:
	band #0,r0l
	band #0,@r0
	band #0,@64:8
	bclr #0,r0l
	bclr #0,@r0
	bclr #0,@64:8
	bclr r1l,r0l
	bclr r1l,@r0
	bclr r1l,@64:8
	biand #0,r0l
	biand #0,@r0
	biand #0,@64:8
	bild #0,r0l
	bild #0,@r0
	bild #0,@64:8

