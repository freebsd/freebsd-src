	.text
	.align 2
	.globl	_start
	.type	_start,@function
_start:
	rts
	nop

	.comm	foo,4,4
	.section	.debug_info,"",@progbits
	.long	0
	.ualong	foo
