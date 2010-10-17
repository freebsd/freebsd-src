! First part of crange-2b.s, but in section .text.mixed.

	.section .text.mixed,"ax"
	.mode SHmedia
	.align 2
sec1:
	nop
	nop
	nop
	nop
sec2:
	.long 41
	.long 43
	.long 42
	.long 43
	.long 42
