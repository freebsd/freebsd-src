	.text
	.globl	_start
	.type	_start, @function
_start:
	jal	foo
	jal	sexternal
	j	sglobal
	.size	_start, .-_start

	.globl	sexternal
	.type	sexternal, @function
sexternal:
	jr	$31
	.size	sexternal, .-sexternal

	.data
	.type	dlocal, @object
dlocal:
	.word	dlocal
	.size	dlocal, .-dlocal

	.globl	dexternal
	.type	dexternal, @object
dexternal:
	.word	dglobal
	.word	dexternal
	.size	dexternal, .-dexternal
