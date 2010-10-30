	.text
	.globl _start
_start:

	.section	.debug_info
	.long	.Ltext
	.long	.Ltext + 2

	.section	.text.exit,"ax"
.Ltext:
	.long	0
