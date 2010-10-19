	.text
	.globl	_start
	.type	_start,@function
_start:
	call	foo@plt
	call	sexternal@plt
	jmp	sglobal@plt
	.size	_start, .-_start

	.globl	sexternal
	.type	sexternal,@function
sexternal:
	ret
	.size	sexternal, .-sexternal
