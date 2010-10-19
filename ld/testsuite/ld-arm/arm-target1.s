# Test the R_ARM_TARGET1 relocation
	.text
	.global _start
_start:
	.word foo(target1)
foo:
