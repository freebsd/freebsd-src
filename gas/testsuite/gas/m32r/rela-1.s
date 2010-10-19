
	.section .text
	bl	label
	bl.l	label
	bl.s	label
	bnez	r0,label
	mv	r0,r0
	bl.s	label

	.section .text2, "ax" 
	nop
	nop
	nop
	nop
label:
	.end


