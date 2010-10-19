	.text
	.globl	foo
	.type	foo, %function
foo:
	save	%sp, -104, %sp
	sethi	%hi(__GOTT_BASE__), %l7
	ld	[%l7+%lo(__GOTT_BASE__)],%l7
	ld	[%l7+%lo(__GOTT_INDEX__)],%l7
	sethi	%hi(x), %g1
	or	%g1, %lo(x), %g1
	ld	[%l7+%g1], %g1
	ld	[%g1], %g2
	add	%g2, 1, %g2

	call	slocal, 0
	st	%g2, [%g1]

	call	sexternal, 0
	nop

	call	sexternal, 0
	nop

	ret
	restore
	.size	foo, .-foo

	.type	slocal, %function
slocal:
	retl
	nop
	.size	slocal, .-slocal

	.globl	sglobal
	.type	sglobal, %function
sglobal:
	retl
	nop
	.size	sglobal, .-sglobal

	.data
	.4byte	slocal

	.comm	x,4,4
