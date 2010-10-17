; Test MRI structured repeat pseudo-op.

	xdef	foo
foo
	repeat
	until	<cs>

	clr d1
	repeat
	  add #1,d1
	until d1 <ge> #10

	nop
	nop
