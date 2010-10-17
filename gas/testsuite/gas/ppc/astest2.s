four	= 4
	.section ".text"
foo:	
	nop ; nop ; nop
	.globl a
	b .+4
	b .+8
	b x
	b y
	b z
	b z+20
	b .+four
	b a
	b b
	b a+4
	b b+4
	b a@local
	b b@local
	.long .
	.long .+8
	.long x-.
	.long x+4-.
	.long z-.
	.long y-.
	.long x
	.long y
	.long z
	.long x-four
	.long y-four
	.long z-four
	.long a-.
	.long b-.
a:	.long a
b:	.long b

apfour	= a + four
	.long apfour
	.long a-apfour
	.long apfour+2
	.long apfour-b

	.section ".data"
	.globl x
	.globl z
x:	.long 0
z	= . + 4
y:	.long 0

	.type	foo,@function
	.type	a,@function
	.type	b,@function
	.type	apfour,@function

	.section ".text"
.L1:
	nop
	ble- 1,.L1
	bgt- 2,.L1
	ble+ 3,.L1
	bgt+ 4,.L1
	ble- 5,.L2
	bgt- 6,.L2
	ble+ 7,.L2
	bgt+ 0,.L2
.L2:
	nop:	
