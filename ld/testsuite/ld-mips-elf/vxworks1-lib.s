	.text
	.globl	foo
	.type	foo, @function
foo:
	addiu	$sp,$sp,-32
	sw	$31,($sp)
	sw	$28,4($sp)
	lui	$28,%hi(__GOTT_BASE__)
	lw	$28,%lo(__GOTT_BASE__)($28)
	lw	$28,%half(__GOTT_INDEX__)($28)
	lw	$2,%got(x)($28)
	lw	$3,($2)
	addiu	$3,$3,1
	sw	$3,($2)
	lw	$25,%got(slocal)($gp)
	jalr	$25
	lw	$25,%call16(sglobal)($gp)
	jalr	$25
	lw	$25,%call16(sexternal)($gp)
	jalr	$25
	lw	$31,($sp)
	lw	$28,4($sp)
	addiu	$sp,$sp,32
	jr	$31
	.size	foo, .-foo

	.type	slocal, @function
slocal:
	jr	$31
	.size	slocal, .-slocal

	.globl	sglobal
	.type	sglobal, @function
sglobal:
	jr	$31
	.size	sglobal, .-sglobal

	.comm	x,4,4

	.data
	.type	dlocal, @object
dlocal:
	.word	slocal
	.word	dlocal
	.size	dlocal, .-dlocal

	.globl	dglobal
	.type	dglobal, @object
dglobal:
	.word	dglobal
	.word	dexternal
	.size	dglobal, .-dglobal
