# Test absolute relocation R_CRX_ABS32

	.section	.text_32,"ax","progbits"
	.global _start
_start:
	loadb foo32, r1

