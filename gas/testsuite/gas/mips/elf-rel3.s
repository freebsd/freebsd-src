	.data
	.type	 x,@object
	.size	 x,4
x:
	.word	0x12121212
	.globl	b
	.type	 b,@object
	.size	 b,8
b:
	.word	b+4
	.word	x
	.word	0
