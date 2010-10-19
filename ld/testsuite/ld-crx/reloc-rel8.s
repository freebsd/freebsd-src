# Test pc relative relocation R_CRX_REL8

	.section	.text_8,"ax","progbits"
	.global _start
	.global foo8
_start:
	beq foo8
foo8:
