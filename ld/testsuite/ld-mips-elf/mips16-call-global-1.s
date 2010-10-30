	.set	mips16

	.globl	__start
	.ent	__start
__start:
	.frame	$sp,24,$31
	save	24,$31
	jal	x+8
	jal	y+8
	restore	24,$31
	j	$31
	.end	__start
