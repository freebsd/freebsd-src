# Test register relative relocation R_CRX_REGREL12

	.section	.text_12,"ax","progbits"
	.global _start
_start:
	loadb foo12(r7)+, r5
