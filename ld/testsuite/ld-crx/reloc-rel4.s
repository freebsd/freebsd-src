# Test pc relative relocation R_CRX_REL4

	.section	.text_4,"ax","progbits"
	.global _start
	.global foo4
_start:
	beq0b r10 , foo4
foo4:
