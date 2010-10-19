	.set noat
	lw $27, 0x7fff($27)
	lw $27, -0x8000($27)
	sw $27, 0x7fff($27)
	sw $27, -0x8000($27)

	.space 8
