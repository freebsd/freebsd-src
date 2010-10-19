	.text
	.globl	_start
	.type	_start, %function
_start:
	save	%sp, -104, %sp

	call	foo, 0
	nop

	call	sexternal, 0
	nop

	call	sglobal, 0
	nop

	ret
	restore
	.size	_start, .-_start

	.globl	sexternal
	.type	sexternal, %function
sexternal:
	retl
	nop
	.size	sexternal, .-sexternal
