# Test pc relative relocation R_CRX_REL16

	.section	.text_16,"ax","progbits"
	.global _start
	.global foo16
_start:
	bal ra, foo16
foo16:
