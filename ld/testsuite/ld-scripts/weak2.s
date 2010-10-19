	.data
	.global	foo2
	.weak	sym1
	.global	sym2
foo2:
	.long	sym1
	.long	sym2
sym1:
	.long	0x56565656
sym2:
	.long	0x78787878
