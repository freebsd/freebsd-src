	.file	1 "elf_e_flags.c"
gcc2_compiled.:
__gnu_compiled_c:
	.text
	.align	2
	.globl	foo
	.text
	.ent	foo
foo:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, extra= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	mul	$2,$4,$5
	.set	noreorder
	.set	nomacro
	j	$31
	addu	$2,$2,1
	.set	macro
	.set	reorder

	.end	foo
	.align	2
	.globl	main
	.text
	.ent	main
main:
	.frame	$sp,40,$31		# vars= 0, regs= 1/0, args= 32, extra= 0
	.mask	0x80000000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,40
	sw	$31,32($sp)
	jal	__gccmain
	move	$2,$0
	lw	$31,32($sp)
	nop
	.set	noreorder
	.set	nomacro
	j	$31
	addu	$sp,$sp,40
	.set	macro
	.set	reorder

	.end	main

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
