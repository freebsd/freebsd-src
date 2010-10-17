# Source file used to test the 20-bit break instructions
foo:	
	break
	break	0
	break	20
	break	20,40
	break	1023,1023
	
	sdbbp
	sdbbp	0
	sdbbp	20
	sdbbp	20,40
	sdbbp	1023,1023
	
# force some padding, to make objdump consistently report that there's some
# here...
	.space	8
