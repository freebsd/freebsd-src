	.section ".data"
	.globl x
	.globl z
x:	.long 0
z	= . + 4
four	= z - x - 4
y:	.long 0
	
	.section ".text"
foo:	
	nop ; nop ; nop
	.globl a
a:	b .+4
b:	b .+8
	b x
	b y
	b z
	b z+20
	b .+four
	b a
	b b
	b a+4
	b b+4
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
	.long a
	.long b

apfour	= a + four
	.long apfour
	.long a-apfour
	.long apfour+2
	.long apfour-b

	.type	foo,@function
	.type	a,@function
	.type	b,@function
	.type	apfour,@function
