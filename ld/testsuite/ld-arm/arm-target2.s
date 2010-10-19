# Test the R_ARM_TARGET2 relocation
	.text
	.global _start
_start:
	.word foo(target2)
foo:
