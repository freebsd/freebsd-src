# Source file used to test the beq macro.
	.text
text_label:	
	beq	$4,$5,text_label
	beq	$4,0,text_label
	beq	$4,1,text_label
	beq	$4,0x8000,text_label
	beq	$4,-0x8000,text_label
	beq	$4,0x10000,text_label
	beq	$4,0x1a5a5,text_label

# bne is handled by the same code as beq.  Just sanity check.
	bne	$4,0,text_label

# Sanity check beql and bnel
	.set	mips2
	beql	$4,0,text_label
	bnel	$4,0,text_label

# Test that branches which overflow are converted to jumps.
	.space	0x20000
	b	text_label
	bal	text_label

# Branch to an external label.
#	b	external_label
#	bal	external_label

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8
