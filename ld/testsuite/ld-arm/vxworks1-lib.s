	.text
	.globl	foo
	.type	foo, %function
foo:
	stmfd	sp!, {r9, lr, pc}
	ldr	r9, 1f
	ldr	r9, [r9]
	ldr	r9, [r9, #__GOTT_INDEX__]
	ldr	r0, 1f + 4
	ldr	r1, [r9, r0]
	add	r1, r1, #1
	str	r1, [r9, r0]
	bl	slocal(PLT)
	bl	sglobal(PLT)
	bl	sexternal(PLT)
	ldmfd	sp!, {r9, pc}
1:
	.word	__GOTT_BASE__
	.word	x(got)
	.size	foo, .-foo

	.type	slocal, %function
slocal:
	mov	pc,lr
	.size	slocal, .-slocal

	.globl	sglobal
	.type	sglobal, %function
sglobal:
	mov	pc,lr
	.size	sglobal, .-sglobal

	.data
	.4byte	slocal

	.comm	x,4,4
