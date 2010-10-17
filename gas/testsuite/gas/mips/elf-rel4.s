
	.section .sdata
	.global a
	.4byte 1
a:	.4byte 2

	.section .text
	la $4,a
	la $4,a+4
	la $4,a+8
	la $4,a+12

