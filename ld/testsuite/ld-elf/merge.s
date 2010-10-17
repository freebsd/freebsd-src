	.section .rodata.str,"aMS","progbits",1
.LC0:
	.asciz	"abc"
.LC1:
	.asciz	"c"

	.text
	.global _start
_start:
	.long	.LC0
.LT0:
	.long	.LC1
	.long	.LC0-.LT0
	.long	.LC1-.LT0
