	.section	.text.foo,"axG",@progbits,foo,comdat
	.align	2
	.weak	foo
	.ent	foo
	.type	foo, @function
foo:
$LFB308:
	.frame	$fp,136,$31		# vars= 72, regs= 10/0, args= 16, gp= 8
	.mask	0xc0ff0000,-4
	.fmask	0x00000000,0
	.set	noreorder
	.cpload	$25

	.set	nomacro
	bne	$3,$0,$L924
	lw	$25,%got($L874)($28)
	.set	macro
	.set	reorder
	lw	$5,%got($LC28)($28)
	lw	$4,136($fp)
	addiu	$5,$5,%lo($LC28)
	lw	$25,%call16(bar)($28)
	.set	noreorder
	.set	nomacro
	jalr	$25
	li	$6,-1			# 0xffffffffffffffff
	.set	macro
	.set	reorder
	lw	$25,64($fp)
	.set	noreorder
	.set	nomacro
	bne	$25,$0,$L846
	lw	$5,%got($LC27)($28)
	b	$L848
	sw	$0,68($fp)
	.set	macro
	.set	reorder
$L920:
	lb	$3,0($18)
	li	$2,59			# 0x3b
	.set	noreorder
	.set	nomacro
	beq	$3,$2,$L925
	lw	$25,76($fp)
	b	$L920
	addiu	$18,$18,1
	.set	macro
	.set	reorder

$L924:
	sll	$2,$2,2
	addiu	$25,$25,%lo($L874)
	addu	$2,$2,$25
	lw	$3,0($2)
	addu	$3,$3,$28
	j	$3
	.end foo
	.section	.rodata.foo,"aG",@progbits,foo,comdat
	.align	2
	.align	2
$L874:
	.gpword	$L924
