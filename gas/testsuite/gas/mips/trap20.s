# Source file used to test the 20-bit trap instructions
foo:	
	teq	$0,$3
	teq	$0,$3,1
	tge	$0,$3
	tge	$0,$3,3
	tgeu	$0,$3
	tgeu	$0,$3,7
	tlt	$0,$3
	tlt	$0,$3,31
	tltu	$0,$3
	tltu	$0,$3,255
	tne	$0,$3
	tne	$0,$3,1023
	
# force some padding, to make objdump consistently report that there's some
# here...
	.space	8
