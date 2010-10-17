	.section .data2,"wa"
	.globl BAR
	.long 0
FOO:	.long 0
BAR:	.long 0
	.long FOO - .
	.long BAR - .

	.data
	.long 0
	.long FOO - .
	.long BAR - .

	.text
	.globl bar
	nop
	br foo
	br bar
	nop
foo:	nop
bar:	nop

	.section .text2,"ax"
	nop
	br foo
	br bar
