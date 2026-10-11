.set noreorder

.section .init
	lw $gp,24($sp)
	lw $ra,28($sp)
	jr $ra
	addu $sp,$sp,32

.section .fini
	lw $gp,24($sp)
	lw $ra,28($sp)
	jr $ra
	addu $sp,$sp,32
