.text
start:	nop
	nop
	nop
	bras label1
	bras label2
.globl label1
label1:	nop
	.space 0xa0
	nop
	nop
.globl label2
label2:	bras label1
	bras label2
	nop
