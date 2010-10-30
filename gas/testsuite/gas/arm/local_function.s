	.text
	.type foo, %function
foo:
	bl bar

	.section .text.bar
	nop
	.type bar, %function
bar:
	nop
