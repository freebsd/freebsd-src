# This test case includes a number of cases where there is no load
# hazard between a load and the instruction which follows it in
# the pipeline.

.data
.text
	lw %0, 0x40(%0)
	add %1, %2, %3
	lh %0, 0x80(%0)
	add %1, %2, %3
	lb %0, 0x80(%0)
	add %1, %2, %3
	lw %0, 0x80(%0)
	nop
	add %0, %0, %0
	lw %0, 0x80(%3)
	nop
	lw %0, 0x80(%3)
	add %2, %3, %4
	
