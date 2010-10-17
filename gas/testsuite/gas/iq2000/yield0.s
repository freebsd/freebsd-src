# This test case includes a single case of a yield instruction
# (e.g. SLEEP) appearing in the branch delay slot.  We expect
# the assembler to issue a warning about this!
	
.text
	# yield insn in the branch delay slot.
	beq %0,%0,foo
	cfc2 %1, %1

	# likewise for the rest.
	beq %0,%0,foo
	cfc3 %1, %1
	
	beq %0,%0,foo
	chkhdr %1, %1

	beq %0,%0,foo
	luc32 %1, %1

	beq %0,%0,foo
	luc32l %1, %1

	beq %0,%0,foo
	luc64 %1, %1

	beq %0,%0,foo
	luc64l %1, %1

	beq %0,%0,foo
	lulck %1

	beq %0,%0,foo
	lum32 %1, %1

	beq %0,%0,foo
	lum32l %1, %1

	beq %0,%0,foo
	lum64 %1, %1

	beq %0,%0,foo
	lum64l %1, %1

	beq %0,%0,foo
	lur %1, %1

	beq %0,%0,foo
	lurl %1, %1

	beq %0,%0,foo
	luulck %1

	beq %0,%0,foo
	mfc2 %1, %1

	beq %0,%0,foo
	mfc3 %1, %1

	beq %0,%0,foo
	rb %1, %1

	beq %0,%0,foo
	rbr1 %1, 1, 1

	beq %0,%0,foo
	rbr30 %1, 1, 1

	beq %0,%0,foo
	rx %1, %1

	beq %0,%0,foo
	rxr1 %1, 1, 1

	beq %0,%0,foo
	rxr30 %1, 1, 1

	beq %0,%0,foo
	sleep

	beq %0,%0,foo
	srrd %1

	beq %0,%0,foo
	srrdl %1
	
	beq %0,%0,foo
	srulck %1

	beq %0,%0,foo
	srwr %1, %1

	beq %0,%0,foo
	srwru %1, %1

	beq %0,%0,foo
	syscall

	beq %0,%0,foo
	trapqfl

	beq %0,%0,foo
	trapqne

	beq %0,%0,foo
	wb %1, %1

	beq %0,%0,foo
	wbu %1, %1

	beq %0,%0,foo
	wbr1 %1, 1, 1

	beq %0,%0,foo
	wbr1u %1, 1, 1

	beq %0,%0,foo
	wbr30 %1, 1, 1

	beq %0,%0,foo
	wbr30u %1, 1, 1

	beq %0,%0,foo
	wx %1, %1

	beq %0,%0,foo
	wxu %1, %1

	beq %0,%0,foo
	wxr1 %1, 1, 1

	beq %0,%0,foo
	wxr1u %1, 1, 1

	beq %0,%0,foo
	wxr30 %1, 1, 1

	beq %0,%0,foo
	wxr30u %1, 1, 1
	
foo:	nop
