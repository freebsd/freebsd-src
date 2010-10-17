; Test MRI structured for pseudo-op.

	xdef	foo
foo
	clr d1
	for d0 = #1 to #10 by #2 do
	  add d0,d1 arbitrary text 'in comment field
	  if <vs> then
	    break
	  endi
	endf

	clr d1
	for d0 = #10 downto #1 do
	  if d1 <ge> #100 then
	    next
	  endi
	  add d0,d1
	endf

	nop
	nop
