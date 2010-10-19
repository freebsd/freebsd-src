# Test immediate relocation R_CRX_IMM32

	.section	.text_32,"ax","progbits"
	.global _start
_start:
	addd $foo32, r6

