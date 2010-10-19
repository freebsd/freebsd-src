# Source file used to test the bgeu macro.
	
text_label:	
	bgeu	$4,$5,text_label
	bgeu	$0,$5,text_label
	# A second argument of 0 or $0 is always true
	bgeu	$4,1,text_label
	bgeu	$4,2,text_label
	bgeu	$4,0x8000,text_label
	bgeu	$4,-0x8000,text_label
	bgeu	$4,0x10000,text_label
	bgeu	$4,0x1a5a5,text_label

# bgtu is handled like bgeu, except when both arguments are registers.
# Just sanity check it otherwise.
	bgtu	$4,$5,text_label
	bgtu	$4,$0,text_label
	bgtu	$4,0,text_label

# Sanity test bgeul and bgtul
	.set	mips2
	bgeul	$4,$5,text_label
	bgtul	$4,$5,text_label

# Branch to an external label.
	bgeu	$4,$5,external_label
	bgtu	$4,$5,external_label
	bgeul	$4,$5,external_label
	bgtul	$4,$5,external_label

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
