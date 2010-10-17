# Dual-mode pairs must be aligned on an 8-byte boundary.  This tests
# that an error is reported if not properly aligned.

	.text
	.align 8
	nop
	d.fadd.ss	%f3,%f5,%f7
	addu	%r4,%r5,%r6

