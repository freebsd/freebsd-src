	.text
	.globl	_start
	.type	_start, %function
_start:
	bl	foo
	bl	sexternal
	b	sglobal
	.size	_start, .-_start

	.globl	sexternal
	.type	sexternal, %function
sexternal:
	mov	pc, lr
	.size	sexternal, .-sexternal
