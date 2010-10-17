; Test MRI structured while pseudo-op.

	xdef	foo
foo
	while <cs> do
	endw

	clr d1
	while d1 <le> #10 do
	  add #1,d1
	endw

	nop
	nop
