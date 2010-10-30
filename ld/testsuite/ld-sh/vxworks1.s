	.text
	.globl	__start
	.type	__start, %function
__start:
	sts.l	pr,@-r15
	mov.l	1f,r0
	jsr	@r0
	nop

	mov.l	2f,r0
	jsr	@r0
	nop

	mov.l	3f,r0
	jsr	@r0
	nop

	lds.l	@r15+,pr
	rts
	nop
	.align	2
1:	.long	_foo
2:	.long	_sglobal
3:	.long	_sexternal
	.size	__start, .-__start

	.globl	_sexternal
	.type	_sexternal, %function
_sexternal:
	rts
	nop
	.size	_sexternal, .-_sexternal
