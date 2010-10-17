	.section .bss1, "aw", "nobits"
	.space 0x10
	.section .bss2, "aw", "nobits"
	.space 0x30
	.section .bss3, "aw", "nobits"
	.space 0x20

	.section .text1, "ax", "progbits"
	.space 0x80
	.section .text2, "ax", "progbits"
	.space 0x40
	.section .text3, "ax", "progbits"
	.space 0x20

	.section .data1, "aw", "progbits"
	.space 0x30
	.section .data2, "aw", "progbits"
	.space 0x40
	.section .data3, "aw", "progbits"
	.space 0x50

	.section .mtext, "ax", "progbits"
	.space 0x20
	.section .mbss, "aw", "nobits"
	.space 0x30
