# Source file used to test the bltu macro.
	
text_label:	
	bltu	$4,$5,text_label
	bltu	$0,$5,text_label
	# A second argument of 0 or $0 is always false
	bltu	$4,1,text_label
	bltu	$4,2,text_label
	bltu	$4,0x8000,text_label
	bltu	$4,-0x8000,text_label
	bltu	$4,0x10000,text_label
	bltu	$4,0x1a5a5,text_label

# bleu is handled like bltu, except when both arguments are registers.
# Just sanity check it otherwise.
	bleu	$4,$5,text_label
	bleu	$4,$0,text_label
	bleu	$4,0,text_label

# Sanity test bltul and bleul
	.set	mips2
	bltul	$4,$5,text_label
	bleul	$4,$5,text_label

# Branch to an external label.
	bltu	$4,$5,external_label
	bleu	$4,$5,external_label
	bltul	$4,$5,external_label
	bleul	$4,$5,external_label

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
