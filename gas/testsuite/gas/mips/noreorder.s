	.text
	.globl	per_cpu_trap_init
	.ent	per_cpu_trap_init
	.type	per_cpu_trap_init, @function
per_cpu_trap_init:
$L807:
	nop
	nop
	# Removing this .align make the code assemble correctly
	.align	3
	jal	cpu_cache_init
	lw	$31,16($sp)
	.set	noreorder
	j	tlb_init
	addiu	$sp,$sp,24
	# Replacing this .word with a nop causes the code to be assembled corrrectly
	.word	0
	# Removing this nop causes the code to be compiled correctly
	nop
	.set	reorder					

	b	$L807
	.end	per_cpu_trap_init

	.p2align 4
