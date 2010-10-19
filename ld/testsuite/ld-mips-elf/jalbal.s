# Test that jal gets converted to bal on the RM9000 when it is in range.
	.text
	.global	s1
	.type	s1,@function
	.set	noreorder
s1:
	jal	s3
	nop
	jal	s3
s2:
	nop
	.space	0x1fff8
s3:
	jal	s2
	nop
	jal	s2
	nop
	nop
