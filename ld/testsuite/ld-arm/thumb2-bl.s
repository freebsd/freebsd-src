@ Test to ensure that a Thumb-2 BL works with an offset that is
@ not permissable for Thumb-1.

	.arch armv7
	.global _start
	.syntax unified

@ We will place the section .text at 0x1000.

	.text
	.thumb_func

_start:
	bl bar

@ We will place the section .foo at 0x1001000.

	.section .foo, "xa"
	.thumb_func

bar:
	bx lr

