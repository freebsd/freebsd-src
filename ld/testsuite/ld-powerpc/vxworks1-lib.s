	.text
	.globl	foo
	.type	foo, @function
foo:
	stwu	r1,-24(r1)
	mflr	r0
	stw	r0,28(r1)
	lis	r30,__GOTT_BASE__@ha
	lwz	r30,__GOTT_BASE__@l(r30)
	lwz	r30,__GOTT_INDEX__(r30)
	lwz	r1,x@got(r30)
	lwz	r0,0(r1)
	addi	r0,r0,1
	stw	r0,0(r1)
	bl	slocal
	bl	sglobal@plt
	bl	sexternal@plt
	lwz	r0,28(r1)
	mtlr	r0
	addi	r1,r1,24
	blr
	.size	foo, .-foo

	.type	slocal, @function
slocal:
	blr
	.size	slocal, .-slocal

	.globl	sglobal
	.type	sglobal, @function
sglobal:
	blr
	.size	sglobal, .-sglobal

	.data
	.4byte	slocal

	.comm	x,4,4
