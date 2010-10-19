	.section .merge1,"aMS",@progbits,1
A:	.string	"flutter"

	.section .merge2,"aMS",@progbits,1
B:	.string "sting"

	.section .merge3,"aM",@progbits,4
C:	.4byte	0x300
D:	.4byte	0x200

	.data
E:	.4byte	E
	.4byte	E + 0x1000
	.4byte	A
	.4byte	B
	.4byte	C
	.4byte	D
