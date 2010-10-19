# Source file used to test the blt macro.
	
text_label:	
	blt	$4,$5,text_label
	blt	$4,$0,text_label
	blt	$0,$5,text_label
	blt	$4,0,text_label
	blt	$4,1,text_label
	blt	$4,2,text_label
	blt	$4,0x8000,text_label
	blt	$4,-0x8000,text_label
	blt	$4,0x10000,text_label
	blt	$4,0x1a5a5,text_label

# ble is handled like blt, except when both arguments are registers.
# Just sanity check it otherwise.
	ble	$4,$5,text_label
	ble	$4,$0,text_label
	ble	$0,$5,text_label
	ble	$4,0,text_label

# Sanity test bltl and blel
	.set	mips2
	bltl	$4,$5,text_label
	blel	$4,$5,text_label

# Branch to an external label.
	blt	$4,$5,external_label
	ble	$4,$5,external_label
	bltl	$4,$5,external_label
	blel	$4,$5,external_label

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
	nop
	nop
