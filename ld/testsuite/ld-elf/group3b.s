	.section .data,"awG",%progbits,foo_group,comdat
	.hidden foo
	.globl foo
	.type foo,%object
foo:
	.word 0
