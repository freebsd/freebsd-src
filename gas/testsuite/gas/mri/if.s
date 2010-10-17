; Test MRI structured if pseudo-op.

	xdef	foo
foo
	if d1 <gt> d0 and d2 <gt> d0 then
	  if d1 <gt> d2 then
	    move d1,d3
	  else
	    move d2,d3
	  endi
	else
	  if d0 <gt> d1 or d0 <gt> d2 then
	    move d0,d3
	  endi
	endi

	nop
