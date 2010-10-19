# Test pc relative relocation R_CRX_REL32

	.section	.text_32,"ax","progbits"
	.global _start
_start:
	br foo32
