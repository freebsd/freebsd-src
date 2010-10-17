	.h8300h
	.globl	_start
_start:
	mov.b	@foo:16,r0l
	mov.b	@bar:32,r0l

	.equ	foo,0xffff67
	.equ	bar,0x4321
