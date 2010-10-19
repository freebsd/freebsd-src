	.text
	.globl	_start
	.type	_start,@function
_start:
	bl	foo@plt
	bl	sexternal@plt
	bl	sglobal@plt
	.size	_start, .-_start

	.globl	sexternal
	.type	sexternal,@function
sexternal:
	blr
	.size	sexternal, .-sexternal
