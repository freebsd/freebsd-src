# This test case includes a number of cases where a yield instruction
# (e.g. SLEEP) does NOT appear in the branch delay slot.

.text
test1:	beq %0, %0, test2
	# nop in the branch delay slot.
	nop
test2:	cfc %0, %0
	nop
test3:	cfc %0, %0
	beq %0, %0, test4
	nop
test4:	sub %1,%2,%3

