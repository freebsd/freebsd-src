	.arm
	.text
	.globl _start
_start:
	fmacseq s9, s14, s1
	flds s14, [r2]
	bx lr
