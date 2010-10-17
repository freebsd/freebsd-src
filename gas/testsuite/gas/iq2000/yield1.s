# This test case includes a single case of a yield instruction
# (e.g. SLEEP) appearing in the branch delay slot.  We expect
# the assembler to issue a warning about this!
	
.text
	jalr %3, %4
	# sleep insn in the branch delay slot.
	sleep
foo:	nop