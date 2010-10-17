.data
	.global	foo1
	.global	sym1
	.weak	sym2
foo1:
	.long	sym1
	.long	sym2
sym1:
	.long	0x12121212
sym2:
	.long	0x34343434
