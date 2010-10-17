# Difference of two undefined symbols.
# The assembler should reject this.
	.text
	.globl _foo
_foo:	.long _a - _b
