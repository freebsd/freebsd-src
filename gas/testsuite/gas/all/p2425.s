	.text
	.globl _frobnitz
_frobnitz:
	.long 1, 2, 3, 4, 5, 6, 7, GRUMP, 42
	GRUMP=.-_frobnitz
	HALFGRUMP=GRUMP/2
