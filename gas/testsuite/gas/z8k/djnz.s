.text
.globl	label1
label1:	nop
	.space	248

	djnz	r0,label1
	dbjnz	rl0,label1
	nop

label2:	nop
	.space	248

	dbjnz	rl0,label2
	djnz	r0,label2
	nop
