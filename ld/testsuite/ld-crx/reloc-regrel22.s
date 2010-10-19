# Test register relative relocation R_CRX_REGREL22

	.section	.text_22,"ax","progbits"
	.global _start
_start:
	loadb foo22(r9,r12,2), r13
