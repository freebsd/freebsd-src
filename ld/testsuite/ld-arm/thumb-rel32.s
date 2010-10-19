	.text
	.arch armv4t
	.global	_start
	.type	_start, %function
	.thumb_func
_start:
	.word bar - .
	.word _start - .
	.byte 0
	.4byte (_start - .) + 1
	.byte 0, 0, 0
	.section .after, "ax", %progbits
	.global bar
	.type bar, %function
	.thumb_func
bar:
	.word 0
	.ident	"GCC: (GNU) 4.1.0 (CodeSourcery ARM)"
