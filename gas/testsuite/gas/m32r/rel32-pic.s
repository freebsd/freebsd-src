	.text
	nop
	nop
bar:
	.section .text2
	.4byte bar - .
label:
	nop
	nop
	.4byte bar - label
	.4byte bar - label2
label2:
