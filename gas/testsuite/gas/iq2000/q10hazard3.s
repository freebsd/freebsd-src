# This test case includes a single case of a load hazard, whereby an
# instruction references a register which is the target of a load.
# The assembler must warn about this!

.data
foodata:
	.word 42

.text
	lw %31, foodata(%1)
	jal %31,footext

footext:
	nop
