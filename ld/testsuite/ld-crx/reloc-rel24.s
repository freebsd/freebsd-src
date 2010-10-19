# Test pc relative relocation R_CRX_REL24

	.section	.text_24,"ax","progbits"
	.global _start
_start:
	cmpbeqb r1, r2, foo24
