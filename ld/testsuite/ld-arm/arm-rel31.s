# Test the R_ARM_REL31 relocation
	.section .before
	.global _start
_start:
	.text
	.rel31 0, foo
	.rel31 0, _start
	.rel31 1, foo
	.rel31 1, _start
	.section .after
foo:
