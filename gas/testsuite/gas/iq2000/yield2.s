# This test case includes a single case of a yield instruction
# (e.g. SLEEP) appearing in the branch delay slot.  We expect
# the assembler to issue a warning about this!
	
.text
	sleep
	beq %0, %0, foo
	# sleep insn in the branch delay slot.
	sleep
foo:	nop
