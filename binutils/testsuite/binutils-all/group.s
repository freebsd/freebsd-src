	.section .text,"axG",%progbits,foo_group,comdat
	.global foo
foo:
	.word 0
	.section .data,"awG",%progbits,foo_group,comdat
	.global bar
bar:
	.word 0
