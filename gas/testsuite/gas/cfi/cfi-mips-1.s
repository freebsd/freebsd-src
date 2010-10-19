	.file	1 "foo.c"
	.section .mdebug.abi64
	.previous
	.text
	.align	2
	.globl	foo
	.ent	foo
	.cfi_startproc
foo:
	.frame	$fp,8,$31		# vars= 8, regs= 1/0, args= 0, gp= 0
	.mask	0x40000000,-8
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro

	.cfi_def_cfa $sp, 0
	addiu	$sp,$sp,-8
	.cfi_adjust_cfa_offset 8
	sw	$fp,0($sp)
	.cfi_offset $30, -8
	move	$fp,$sp
	.cfi_def_cfa $fp, 8

	nop
	nop
	nop
	
	move	$sp,$fp
	lw	$fp,0($sp)
	addiu	$sp,$sp,8
	.cfi_def_cfa $sp, 0
	j	$31
	nop
	.set	macro
	.set	reorder
	.end	foo
	.cfi_endproc
	.size   foo, .-foo
	.ident	"GCC: (GNU) 4.0.0 20041226 (experimental)"
